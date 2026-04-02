#include "heic_native_core.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

#include <libheif/heif.h>
#include <png.h>

// Metadata extracted from a HEIC image.
struct HeicMetadata {
  std::vector<uint8_t> icc_profile;
  std::vector<uint8_t> exif_data;
};

// Custom write callback state for in-memory PNG encoding.
struct PngWriteState {
  std::vector<uint8_t>* buffer;
  bool error = false;
};

static void png_write_to_vector(png_structp png_ptr, png_bytep data,
                                png_size_t length) {
  PngWriteState* state =
      reinterpret_cast<PngWriteState*>(png_get_io_ptr(png_ptr));
  try {
    state->buffer->insert(state->buffer->end(), data, data + length);
  } catch (...) {
    state->error = true;
  }
}

static void png_flush_noop(png_structp /*png_ptr*/) {}

// Decode a HEIC file into raw RGBA pixels.
// Returns true on success; fills out width, height, stride, and pixels.
// Caller must call heif_image_release(out_img) and
// heif_image_handle_release(out_handle) when done.
static bool decode_heic(const char* input_path,
                        heif_context** out_ctx,
                        heif_image_handle** out_handle,
                        heif_image** out_img,
                        int* out_width,
                        int* out_height,
                        int* out_stride,
                        const uint8_t** out_pixels,
                        bool* out_has_alpha) {
  heif_context* ctx = heif_context_alloc();
  if (!ctx) return false;

  heif_error err = heif_context_read_from_file(ctx, input_path, nullptr);
  if (err.code != heif_error_Ok) {
    heif_context_free(ctx);
    return false;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    heif_context_free(ctx);
    return false;
  }

  bool has_alpha = heif_image_handle_has_alpha_channel(handle);

  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB,
                          has_alpha ? heif_chroma_interleaved_RGBA
                                    : heif_chroma_interleaved_RGB,
                          nullptr);
  if (err.code != heif_error_Ok) {
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return false;
  }

  int stride = 0;
  const uint8_t* pixels =
      heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
  if (!pixels) {
    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return false;
  }

  *out_ctx = ctx;
  *out_handle = handle;
  *out_img = img;
  *out_width = heif_image_handle_get_width(handle);
  *out_height = heif_image_handle_get_height(handle);
  *out_stride = stride;
  *out_pixels = pixels;
  *out_has_alpha = has_alpha;
  return true;
}

// Normalize the EXIF orientation tag (274) to 1 (Normal) in a raw EXIF blob.
// The blob has a 4-byte big-endian TIFF-offset header (as returned by libheif).
// Returns true if the tag was found and patched; false if not found (not an error).
static bool normalize_exif_orientation(std::vector<uint8_t>& exif_data) {
  if (exif_data.size() < 8) return false;

  // Skip the 4-byte header offset that libheif prepends.
  uint32_t hdr_offset =
      (static_cast<uint32_t>(exif_data[0]) << 24) |
      (static_cast<uint32_t>(exif_data[1]) << 16) |
      (static_cast<uint32_t>(exif_data[2]) << 8) |
      static_cast<uint32_t>(exif_data[3]);
  size_t tiff_start = 4 + static_cast<size_t>(hdr_offset);
  if (tiff_start + 8 > exif_data.size()) return false;

  // Determine byte order.
  bool little_endian = (exif_data[tiff_start] == 0x49);

  auto read16 = [&](size_t off) -> uint16_t {
    if (off + 2 > exif_data.size()) return 0;
    if (little_endian)
      return static_cast<uint16_t>(exif_data[off]) |
             (static_cast<uint16_t>(exif_data[off + 1]) << 8);
    return (static_cast<uint16_t>(exif_data[off]) << 8) |
           static_cast<uint16_t>(exif_data[off + 1]);
  };

  auto read32 = [&](size_t off) -> uint32_t {
    if (off + 4 > exif_data.size()) return 0;
    if (little_endian)
      return static_cast<uint32_t>(exif_data[off]) |
             (static_cast<uint32_t>(exif_data[off + 1]) << 8) |
             (static_cast<uint32_t>(exif_data[off + 2]) << 16) |
             (static_cast<uint32_t>(exif_data[off + 3]) << 24);
    return (static_cast<uint32_t>(exif_data[off]) << 24) |
           (static_cast<uint32_t>(exif_data[off + 1]) << 16) |
           (static_cast<uint32_t>(exif_data[off + 2]) << 8) |
           static_cast<uint32_t>(exif_data[off + 3]);
  };

  auto write16 = [&](size_t off, uint16_t val) {
    if (off + 2 > exif_data.size()) return;
    if (little_endian) {
      exif_data[off] = static_cast<uint8_t>(val & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    } else {
      exif_data[off] = static_cast<uint8_t>((val >> 8) & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>(val & 0xFF);
    }
  };

  // IFD0 offset is at bytes 4-7 of the TIFF block (relative to tiff_start).
  uint32_t ifd0_offset = read32(tiff_start + 4);
  size_t ifd0_pos = tiff_start + ifd0_offset;
  if (ifd0_pos + 2 > exif_data.size()) return false;

  uint16_t entry_count = read16(ifd0_pos);
  for (uint16_t i = 0; i < entry_count; i++) {
    size_t entry_pos = ifd0_pos + 2 + static_cast<size_t>(i) * 12;
    if (entry_pos + 12 > exif_data.size()) break;
    uint16_t tag = read16(entry_pos);
    if (tag == 0x0112) {  // Orientation
      // Type must be SHORT (3), count must be 1.
      // Value is at entry_pos + 8 (4-byte value field, SHORT in first 2 bytes).
      write16(entry_pos + 8, 1);  // 1 = Normal
      return true;
    }
  }
  return false;
}

// Patch EXIF dimension tags to match the decoded (possibly rotated) image size.
// Updates IFD0 tags 0x0100/0x0101 and ExifIFD tags 0xA002/0xA003.
static void update_exif_dimensions(std::vector<uint8_t>& exif_data,
                                   int width, int height) {
  if (exif_data.size() < 8) return;

  uint32_t hdr_offset =
      (static_cast<uint32_t>(exif_data[0]) << 24) |
      (static_cast<uint32_t>(exif_data[1]) << 16) |
      (static_cast<uint32_t>(exif_data[2]) << 8) |
      static_cast<uint32_t>(exif_data[3]);
  size_t tiff_start = 4 + static_cast<size_t>(hdr_offset);
  if (tiff_start + 8 > exif_data.size()) return;

  bool little_endian = (exif_data[tiff_start] == 0x49);

  auto read16 = [&](size_t off) -> uint16_t {
    if (off + 2 > exif_data.size()) return 0;
    if (little_endian)
      return static_cast<uint16_t>(exif_data[off]) |
             (static_cast<uint16_t>(exif_data[off + 1]) << 8);
    return (static_cast<uint16_t>(exif_data[off]) << 8) |
           static_cast<uint16_t>(exif_data[off + 1]);
  };

  auto read32 = [&](size_t off) -> uint32_t {
    if (off + 4 > exif_data.size()) return 0;
    if (little_endian)
      return static_cast<uint32_t>(exif_data[off]) |
             (static_cast<uint32_t>(exif_data[off + 1]) << 8) |
             (static_cast<uint32_t>(exif_data[off + 2]) << 16) |
             (static_cast<uint32_t>(exif_data[off + 3]) << 24);
    return (static_cast<uint32_t>(exif_data[off]) << 24) |
           (static_cast<uint32_t>(exif_data[off + 1]) << 16) |
           (static_cast<uint32_t>(exif_data[off + 2]) << 8) |
           static_cast<uint32_t>(exif_data[off + 3]);
  };

  auto write16 = [&](size_t off, uint16_t val) {
    if (off + 2 > exif_data.size()) return;
    if (little_endian) {
      exif_data[off] = static_cast<uint8_t>(val & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    } else {
      exif_data[off] = static_cast<uint8_t>((val >> 8) & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>(val & 0xFF);
    }
  };

  auto write32 = [&](size_t off, uint32_t val) {
    if (off + 4 > exif_data.size()) return;
    if (little_endian) {
      exif_data[off] = static_cast<uint8_t>(val & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
      exif_data[off + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
      exif_data[off + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    } else {
      exif_data[off] = static_cast<uint8_t>((val >> 24) & 0xFF);
      exif_data[off + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
      exif_data[off + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
      exif_data[off + 3] = static_cast<uint8_t>(val & 0xFF);
    }
  };

  uint32_t ifd0_offset = read32(tiff_start + 4);
  size_t ifd0_pos = tiff_start + ifd0_offset;
  if (ifd0_pos + 2 > exif_data.size()) return;

  uint16_t entry_count = read16(ifd0_pos);
  size_t exif_ifd_pos = 0;

  for (uint16_t i = 0; i < entry_count; i++) {
    size_t entry_pos = ifd0_pos + 2 + static_cast<size_t>(i) * 12;
    if (entry_pos + 12 > exif_data.size()) break;
    uint16_t tag = read16(entry_pos);
    uint16_t type = read16(entry_pos + 2);
    if (tag == 0x0100 || tag == 0x0101) {  // ImageWidth / ImageLength
      uint32_t val = static_cast<uint32_t>(tag == 0x0100 ? width : height);
      if (type == 3) {         // SHORT
        write16(entry_pos + 8, static_cast<uint16_t>(val));
      } else if (type == 4) {  // LONG
        write32(entry_pos + 8, val);
      }
    } else if (tag == 0x8769) {  // ExifIFDPointer
      uint32_t exif_ifd_offset = read32(entry_pos + 8);
      exif_ifd_pos = tiff_start + exif_ifd_offset;
    }
  }

  if (exif_ifd_pos != 0 && exif_ifd_pos + 2 <= exif_data.size()) {
    uint16_t exif_entry_count = read16(exif_ifd_pos);
    for (uint16_t i = 0; i < exif_entry_count; i++) {
      size_t entry_pos = exif_ifd_pos + 2 + static_cast<size_t>(i) * 12;
      if (entry_pos + 12 > exif_data.size()) break;
      uint16_t tag = read16(entry_pos);
      uint16_t type = read16(entry_pos + 2);
      if (tag == 0xA002 || tag == 0xA003) {  // PixelXDimension / PixelYDimension
        uint32_t val = static_cast<uint32_t>(tag == 0xA002 ? width : height);
        if (type == 3) {         // SHORT
          write16(entry_pos + 8, static_cast<uint16_t>(val));
        } else if (type == 4) {  // LONG
          write32(entry_pos + 8, val);
        }
      }
    }
  }
}

// Extract ICC profile and EXIF metadata from a decoded HEIC image handle.
static HeicMetadata extract_metadata(heif_image_handle* handle,
                                     int width, int height) {
  HeicMetadata meta;

  // ICC profile
  heif_color_profile_type profile_type =
      heif_image_handle_get_color_profile_type(handle);
  if (profile_type == heif_color_profile_type_prof ||
      profile_type == heif_color_profile_type_rICC) {
    size_t profile_size =
        heif_image_handle_get_raw_color_profile_size(handle);
    if (profile_size > 0) {
      meta.icc_profile.resize(profile_size);
      heif_error err = heif_image_handle_get_raw_color_profile(
          handle, meta.icc_profile.data());
      if (err.code != heif_error_Ok) {
        meta.icc_profile.clear();
      }
    }
  }

  // EXIF data
  heif_item_id exif_id;
  int n = heif_image_handle_get_list_of_metadata_block_IDs(
      handle, "Exif", &exif_id, 1);
  if (n > 0) {
    size_t exif_size =
        heif_image_handle_get_metadata_size(handle, exif_id);
    if (exif_size > 0) {
      meta.exif_data.resize(exif_size);
      heif_error err = heif_image_handle_get_metadata(
          handle, exif_id, meta.exif_data.data());
      if (err.code != heif_error_Ok) {
        meta.exif_data.clear();
      }
    }
  }

  if (!meta.exif_data.empty()) {
    normalize_exif_orientation(meta.exif_data);
    update_exif_dimensions(meta.exif_data, width, height);
  }

  return meta;
}

// Write RGBA pixels as a PNG to an already-opened FILE*.
// Returns true on success.
static bool write_png_to_file(FILE* fp, int width, int height, int stride,
                              const uint8_t* pixels, int compression_level,
                              const HeicMetadata* metadata, bool has_alpha) {
  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) return false;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    return false;
  }

  // SAFETY: libpng uses setjmp/longjmp for error handling. All locals that
  // must be cleaned up on error are either trivially destructible (plain
  // pointers/ints) or are not declared between here and the longjmp target,
  // so C++ destructors are not skipped in a way that would cause leaks.
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8,
               has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_set_compression_level(png_ptr, compression_level);
  if (compression_level == 0) {
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
  } else {
    png_set_filter(png_ptr, 0, PNG_ALL_FILTERS);
  }

  if (metadata) {
    if (!metadata->icc_profile.empty()) {
      png_set_iCCP(png_ptr, info_ptr, "icc",
                   PNG_COMPRESSION_TYPE_BASE,
                   const_cast<png_bytep>(metadata->icc_profile.data()),
                   static_cast<png_uint_32>(metadata->icc_profile.size()));
    }
#if PNG_LIBPNG_VER >= 10632
    if (metadata->exif_data.size() > 4) {
      // First 4 bytes are a big-endian TIFF header offset.
      uint32_t tiff_offset =
          (static_cast<uint32_t>(metadata->exif_data[0]) << 24) |
          (static_cast<uint32_t>(metadata->exif_data[1]) << 16) |
          (static_cast<uint32_t>(metadata->exif_data[2]) << 8) |
          static_cast<uint32_t>(metadata->exif_data[3]);
      if (tiff_offset <= metadata->exif_data.size() - 4) {
        size_t skip = 4 + static_cast<size_t>(tiff_offset);
        png_set_eXIf_1(png_ptr, info_ptr,
                       static_cast<png_uint_32>(metadata->exif_data.size() - skip),
                       const_cast<png_bytep>(metadata->exif_data.data() + skip));
      }
    }
#endif
  }

  png_write_info(png_ptr, info_ptr);

  for (int y = 0; y < height; y++) {
    png_write_row(png_ptr,
                  const_cast<png_bytep>(pixels + static_cast<size_t>(y) * stride));
  }

  png_write_end(png_ptr, nullptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return true;
}

// Write RGBA pixels as a PNG into a memory buffer.
// Returns true on success.
static bool write_png_to_memory(std::vector<uint8_t>* buf, int width,
                                int height, int stride,
                                const uint8_t* pixels, int compression_level,
                                const HeicMetadata* metadata, bool has_alpha) {
  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) return false;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    return false;
  }

  // SAFETY: libpng uses setjmp/longjmp for error handling. All locals that
  // must be cleaned up on error are either trivially destructible (plain
  // pointers/ints) or are not declared between here and the longjmp target,
  // so C++ destructors are not skipped in a way that would cause leaks.
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }

  PngWriteState state{buf};
  png_set_write_fn(png_ptr, &state, png_write_to_vector, png_flush_noop);

  png_set_IHDR(png_ptr, info_ptr, width, height, 8,
               has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_set_compression_level(png_ptr, compression_level);
  if (compression_level == 0) {
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
  } else {
    png_set_filter(png_ptr, 0, PNG_ALL_FILTERS);
  }

  if (metadata) {
    if (!metadata->icc_profile.empty()) {
      png_set_iCCP(png_ptr, info_ptr, "icc",
                   PNG_COMPRESSION_TYPE_BASE,
                   const_cast<png_bytep>(metadata->icc_profile.data()),
                   static_cast<png_uint_32>(metadata->icc_profile.size()));
    }
#if PNG_LIBPNG_VER >= 10632
    if (metadata->exif_data.size() > 4) {
      // First 4 bytes are a big-endian TIFF header offset.
      uint32_t tiff_offset =
          (static_cast<uint32_t>(metadata->exif_data[0]) << 24) |
          (static_cast<uint32_t>(metadata->exif_data[1]) << 16) |
          (static_cast<uint32_t>(metadata->exif_data[2]) << 8) |
          static_cast<uint32_t>(metadata->exif_data[3]);
      if (tiff_offset <= metadata->exif_data.size() - 4) {
        size_t skip = 4 + static_cast<size_t>(tiff_offset);
        png_set_eXIf_1(png_ptr, info_ptr,
                       static_cast<png_uint_32>(metadata->exif_data.size() - skip),
                       const_cast<png_bytep>(metadata->exif_data.data() + skip));
      }
    }
#endif
  }

  png_write_info(png_ptr, info_ptr);
  if (state.error) png_error(png_ptr, "memory allocation failed");

  for (int y = 0; y < height; y++) {
    png_write_row(png_ptr,
                  const_cast<png_bytep>(pixels + static_cast<size_t>(y) * stride));
    if (state.error) png_error(png_ptr, "memory allocation failed");
  }

  png_write_end(png_ptr, nullptr);
  if (state.error) png_error(png_ptr, "memory allocation failed");
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return true;
}

// ---- High-level API functions -----------------------------------------------

bool heic_native_convert(const char* input_path,
                      const char* output_path,
                      int compression_level,
                      bool preserve_metadata,
                      std::string& error_code,
                      std::string& error_message) {
  heif_context* ctx = nullptr;
  heif_image_handle* handle = nullptr;
  heif_image* img = nullptr;
  int width = 0, height = 0, stride = 0;
  const uint8_t* pixels = nullptr;
  bool has_alpha = false;

  bool ok = decode_heic(input_path, &ctx, &handle, &img, &width, &height,
                        &stride, &pixels, &has_alpha);
  if (!ok) {
    error_code = "decode_failed";
    error_message = "Failed to decode HEIC file";
    return false;
  }

  HeicMetadata meta;
  if (preserve_metadata) {
    meta = extract_metadata(handle, width, height);
  }

  char temp_path[PATH_MAX];
  int snp_ret = snprintf(temp_path, sizeof(temp_path),
                         "%s.heic_native.XXXXXX", output_path);
  if (snp_ret < 0 || static_cast<size_t>(snp_ret) >= sizeof(temp_path)) {
    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    error_code = "encode_failed";
    error_message = "Output path too long";
    return false;
  }

  int fd = mkstemp(temp_path);
  bool success = false;
  if (fd >= 0) {
    FILE* fp = fdopen(fd, "wb");
    if (fp) {
      success = write_png_to_file(fp, width, height, stride, pixels,
                                  compression_level,
                                  preserve_metadata ? &meta : nullptr,
                                  has_alpha);
      fclose(fp);
    } else {
      close(fd);
    }
    if (success) {
      if (rename(temp_path, output_path) != 0) {
        unlink(temp_path);
        success = false;
      }
    } else {
      unlink(temp_path);
    }
  }

  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  if (!success) {
    error_code = "encode_failed";
    error_message = "Failed to write PNG file";
    return false;
  }

  return true;
}

bool heic_native_convert_to_bytes(const char* input_path,
                               int compression_level,
                               bool preserve_metadata,
                               std::vector<uint8_t>& out_bytes,
                               std::string& error_code,
                               std::string& error_message) {
  heif_context* ctx = nullptr;
  heif_image_handle* handle = nullptr;
  heif_image* img = nullptr;
  int width = 0, height = 0, stride = 0;
  const uint8_t* pixels = nullptr;
  bool has_alpha = false;

  bool ok = decode_heic(input_path, &ctx, &handle, &img, &width, &height,
                        &stride, &pixels, &has_alpha);
  if (!ok) {
    error_code = "decode_failed";
    error_message = "Failed to decode HEIC file";
    return false;
  }

  HeicMetadata meta;
  if (preserve_metadata) {
    meta = extract_metadata(handle, width, height);
  }

  std::vector<uint8_t> buf;
  bool success = write_png_to_memory(&buf, width, height, stride, pixels,
                                     compression_level,
                                     preserve_metadata ? &meta : nullptr,
                                     has_alpha);

  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  if (!success || buf.empty()) {
    error_code = "encode_failed";
    error_message = "Failed to encode PNG";
    return false;
  }

  out_bytes = std::move(buf);
  return true;
}

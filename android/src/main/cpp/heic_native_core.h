#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Convert a HEIC file to a PNG file.
// Returns true on success, false on failure.
// On failure, error_code and error_message are populated.
bool heic_native_convert(const char* input_path,
                         const char* output_path,
                         int compression_level,
                         bool preserve_metadata,
                         std::string& error_code,
                         std::string& error_message);

// Convert a HEIC file to PNG bytes in memory.
// Returns true on success, false on failure.
// On success, out_bytes contains the PNG data.
// On failure, error_code and error_message are populated.
bool heic_native_convert_to_bytes(const char* input_path,
                                  int compression_level,
                                  bool preserve_metadata,
                                  std::vector<uint8_t>& out_bytes,
                                  std::string& error_code,
                                  std::string& error_message);

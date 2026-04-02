import 'dart:typed_data';

import 'heic_native_platform_interface.dart';

/// A Flutter plugin that converts HEIC/HEIF images to PNG format.
///
/// Provides static methods for converting HEIC/HEIF images to PNG,
/// either as files on disk or as in-memory byte arrays.
class HeicNative {
  /// Converts a HEIC/HEIF image file to PNG format.
  ///
  /// Returns `true` on success. Throws [PlatformException] on failure with
  /// one of the following error codes:
  /// - `invalid_arguments`. missing or invalid inputPath/outputPath.
  /// - `decode_failed`. could not decode the HEIC file (missing file, invalid
  ///   format, or unavailable codec).
  /// - `encode_failed`. could not write the PNG output (I/O error, permission
  ///   denied).
  ///
  /// [compressionLevel] controls PNG compression (0-9, default: 6).
  /// - **Linux/Windows/Android**: Full zlib compression level control
  ///   (0 = fastest, 9 = smallest).
  /// - **iOS/macOS**: 0 = no PNG row filter (fastest), 1-9 = adaptive
  ///   row filter (better compression). The zlib level is fixed internally.
  ///
  /// [preserveMetadata] when true (default), preserves ICC color
  /// profiles and EXIF data from the source HEIC file.
  /// - **Android/Windows**: Metadata transfer is best-effort; native code
  ///   failures are non-fatal and silently skipped.
  static Future<bool> convert(
    String inputPath,
    String outputPath, {
    int compressionLevel = 6,
    bool preserveMetadata = true,
  }) {
    if (inputPath.isEmpty) {
      throw ArgumentError.value(inputPath, 'inputPath', 'Must not be empty');
    }
    if (outputPath.isEmpty) {
      throw ArgumentError.value(outputPath, 'outputPath', 'Must not be empty');
    }
    return HeicNativePlatform.instance.convert(
      inputPath,
      outputPath,
      compressionLevel: compressionLevel,
      preserveMetadata: preserveMetadata,
    );
  }

  /// Converts a HEIC/HEIF image to PNG bytes in memory.
  ///
  /// Returns the PNG bytes on success. Throws [PlatformException] on failure
  /// (see [convert] for error codes). Parameters and platform caveats for
  /// [preserveMetadata] and [compressionLevel] are the same as [convert].
  static Future<Uint8List> convertToBytes(
    String inputPath, {
    int compressionLevel = 6,
    bool preserveMetadata = true,
  }) {
    if (inputPath.isEmpty) {
      throw ArgumentError.value(inputPath, 'inputPath', 'Must not be empty');
    }
    return HeicNativePlatform.instance.convertToBytes(
      inputPath,
      compressionLevel: compressionLevel,
      preserveMetadata: preserveMetadata,
    );
  }
}

import 'dart:typed_data';

import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'heic_native_method_channel.dart';

/// The interface that platform-specific implementations of heic_native must extend.
abstract class HeicNativePlatform extends PlatformInterface {
  /// Creates a new [HeicNativePlatform].
  HeicNativePlatform() : super(token: _token);

  static final Object _token = Object();

  static HeicNativePlatform _instance = MethodChannelHeicNative();

  /// The current platform-specific implementation of [HeicNativePlatform].
  static HeicNativePlatform get instance => _instance;

  /// Sets the platform-specific implementation to use.
  static set instance(HeicNativePlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  /// Converts a HEIC/HEIF file at [inputPath] to PNG at [outputPath].
  Future<bool> convert(
    String inputPath,
    String outputPath, {
    int compressionLevel = 6,
    bool preserveMetadata = true,
  }) {
    throw UnimplementedError('convert() has not been implemented.');
  }

  /// Converts a HEIC/HEIF file at [inputPath] to PNG bytes in memory.
  Future<Uint8List> convertToBytes(
    String inputPath, {
    int compressionLevel = 6,
    bool preserveMetadata = true,
  }) {
    throw UnimplementedError('convertToBytes() has not been implemented.');
  }
}

## 0.1.0

* Initial release of `heic_native` — a Flutter plugin that converts HEIC/HEIF images to PNG using native platform APIs.
* Fully offline, on-device conversion with no third-party services.
* Supports all 5 platforms:
  - **iOS** (CoreGraphics/ImageIO, iOS 12.0+)
  - **Android** (libheif + libpng via NDK, API 28+)
  - **macOS** (CoreGraphics/ImageIO, macOS 10.13+)
  - **Windows** (libheif + libpng, Windows 10 1809+)
  - **Linux** (libheif + libpng, requires libheif-dev and libpng-dev)
* Two conversion modes: file-to-file (`convert`) and file-to-bytes (`convertToBytes`).
* Configurable PNG compression level (0–9).
* Metadata preservation (ICC color profiles, EXIF data) enabled by default, with option to strip.
* Bundled prebuilt native libraries for Android, Windows, and Linux — works out of the box from pub.dev.

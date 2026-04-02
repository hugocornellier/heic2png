# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "heic_native")
project(${PROJECT_NAME} LANGUAGES CXX)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "heic_native_plugin")

# --- Bundled native dependencies (libheif, libde265, libpng, zlib) ---
# These are committed to the repo and included in the pub.dev package.
# To rebuild: trigger the build-native-deps.yml workflow, download the
# artifact, and commit the updated windows/third_party/ directory.
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

if(NOT EXISTS "${THIRD_PARTY_DIR}/include/libheif/heif.h")
  message(FATAL_ERROR
    "Bundled libheif headers not found at ${THIRD_PARTY_DIR}/include/libheif/heif.h\n"
    "Run the build-native-deps CI workflow, download the artifact, and commit windows/third_party/.")
endif()

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "heic_native_plugin.cpp"
  "heic_native_plugin.h"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/heic_native/heic_native_plugin_c_api.h"
  "heic_native_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL _CRT_SECURE_NO_WARNINGS)
# C4611: setjmp/longjmp interaction with C++ destructors is expected (libpng error handling).
target_compile_options(${PLUGIN_NAME} PRIVATE /wd4611)

# Source include directories and library dependencies.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${THIRD_PARTY_DIR}/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin
  comctl32
  "${THIRD_PARTY_DIR}/lib/heif.lib"
  "${THIRD_PARTY_DIR}/lib/de265.lib"
  "${THIRD_PARTY_DIR}/lib/libpng16.lib"
  "${THIRD_PARTY_DIR}/lib/zlib.lib"
)

# List of absolute paths to libraries that should be bundled with the plugin.
# Flutter copies these into the app bundle so they are available at runtime.
set(heic_native_bundled_libraries
  "${THIRD_PARTY_DIR}/bin/heif.dll"
  "${THIRD_PARTY_DIR}/bin/libde265.dll"
  "${THIRD_PARTY_DIR}/bin/libpng16.dll"
  "${THIRD_PARTY_DIR}/bin/zlib1.dll"
  PARENT_SCOPE
)

# Note: Native C++ unit tests have been removed in favor of the comprehensive
# Dart integration tests which test actual HEIC-to-PNG conversion on all platforms.

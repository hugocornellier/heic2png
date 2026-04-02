#include <flutter_linux/flutter_linux.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/heic_native/heic_native_plugin.h"
#include "heic_native_plugin_private.h"

// Unit tests for the heic_native Linux plugin.
// Build and run from the example app's build directory:
//   build/linux/x64/debug/plugins/heic_native/heic_native_test

namespace heic_native {
namespace test {

TEST(HeicNativePlugin, PluginRegistrarExists) {
  // Smoke test: the plugin type must be registered.
  EXPECT_NE(heic_native_plugin_get_type(), 0u);
}

}  // namespace test
}  // namespace heic_native

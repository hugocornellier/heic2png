#include "include/heic_native/heic_native_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "heic_native_plugin.h"

void HeicNativePluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  heic_native::HeicNativePlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

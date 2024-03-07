#include "../include/trimmer/trimmer_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "trimmer_plugin.h"

void TrimmerPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  trimmer::TrimmerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

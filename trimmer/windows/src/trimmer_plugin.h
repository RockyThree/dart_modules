#ifndef FLUTTER_PLUGIN_TRIMMER_PLUGIN_H_
#define FLUTTER_PLUGIN_TRIMMER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace trimmer
{

    class TrimmerPlugin : public flutter::Plugin
    {
    public:
        static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

        TrimmerPlugin();

        virtual ~TrimmerPlugin();

        // Disallow copy and assign.
        TrimmerPlugin(const TrimmerPlugin &) = delete;
        TrimmerPlugin &operator=(const TrimmerPlugin &) = delete;

        // Called when a method is called on this plugin's channel from Dart.
        void HandleMethodCall(
            const flutter::MethodCall<flutter::EncodableValue> &method_call,
            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
    };

} // namespace trimmer

#endif // FLUTTER_PLUGIN_TRIMMER_PLUGIN_H_

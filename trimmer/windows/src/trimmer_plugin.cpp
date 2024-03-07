#include "trimmer_plugin.h"
#pragma warning(disable : 4819)
#include "ffmpeg/VideoDecoder.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>
#include <cstdio>

#pragma warning(disable : 4244 4819)
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace trimmer
{

  // static
  void TrimmerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarWindows *registrar)
  {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "trimmer",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<TrimmerPlugin>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result)
        {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  TrimmerPlugin::TrimmerPlugin() {}

  TrimmerPlugin::~TrimmerPlugin() {}

  void TrimmerPlugin::HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
  {
    if (method_call.method_name().compare("getPlatformVersion") == 0)
    {
      std::ostringstream version_stream;
      version_stream << "Windows ";
      if (IsWindows10OrGreater())
      {
        version_stream << "10+";
      }
      else if (IsWindows8OrGreater())
      {
        version_stream << "8";
      }
      else if (IsWindows7OrGreater())
      {
        version_stream << "7";
      }
      version_stream << " LIBAVCODEC_VERSION ";
      unsigned codecVer = avcodec_version();
      version_stream << "" << codecVer << "";
      // version_stream << LIBAVCODEC_VERSION  << " / ";
      result->Success(flutter::EncodableValue(version_stream.str()));
    }
    else if (method_call.method_name().compare("data") == 0)
    {
      std::string video;
      std::map<std::string, std::string> headers;
      int format = 0; // 默认值为0，可能需要根据实际情况调整
      int maxHeight = 0;
      int maxWidth = 0;
      int timeMs = 0;
      int quality = 10;

      // 获取视频文件名
      if (method_call.arguments())
      {
        const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
        const auto &video_value = arguments_map.find(flutter::EncodableValue("video"));
        video = std::get<std::string>(video_value->second);

        // 获取可选的HTTP请求头
        const auto &headers_value = arguments_map.find(flutter::EncodableValue("headers"));
        if (!headers_value->second.IsNull())
        {
          const auto &headers_map = std::get<flutter::EncodableMap>(headers_value->second);
          for (const auto &entry : headers_map)
          {
            if (!entry.first.IsNull() && !entry.second.IsNull())
            {
              headers[std::get<std::string>(entry.first)] = std::get<std::string>(entry.second);
            }
          }
        }

        // 获取图像格式
        const auto &format_value = arguments_map.find(flutter::EncodableValue("format"));
        if (format_value != arguments_map.end() && !format_value->second.IsNull())
        {
          format = std::get<int>(format_value->second);
        }

        // 获取最大高度
        const auto &maxHeight_value = arguments_map.find(flutter::EncodableValue("maxh"));
        if (maxHeight_value != arguments_map.end() && !maxHeight_value->second.IsNull())
        {
          maxHeight = std::get<int>(maxHeight_value->second);
        }

        // 获取最大宽度
        const auto &maxWidth_value = arguments_map.find(flutter::EncodableValue("maxw"));
        if (maxWidth_value != arguments_map.end() && !maxWidth_value->second.IsNull())
        {
          maxWidth = std::get<int>(maxWidth_value->second);
        }

        // 获取时间戳
        const auto &timeMs_value = arguments_map.find(flutter::EncodableValue("timeMs"));
        if (timeMs_value != arguments_map.end() && !timeMs_value->second.IsNull())
        {
          timeMs = std::get<int>(timeMs_value->second);
        }

        // 获取图像质量
        const auto &quality_value = arguments_map.find(flutter::EncodableValue("quality"));
        if (quality_value != arguments_map.end() && !quality_value->second.IsNull())
        {
          quality = std::get<int>(quality_value->second);
        }
      }

      // 在这里可以使用获取到的参数执行相应的操作
      std::cout << "Video: " << video << std::endl;
      std::cout << "Format: " << format << std::endl;
      std::cout << "Max Height: " << maxHeight << std::endl;
      std::cout << "Max Width: " << maxWidth << std::endl;
      std::cout << "Time (ms): " << timeMs << std::endl;
      std::cout << "Quality: " << quality << std::endl;

      std::cout << "Headers:\n";
      for (const auto &entry : headers)
      {
        std::cout << entry.first << ": " << entry.second << std::endl;
      }

      std::string outputPrefix = "test";
      if (!video.empty() && !outputPrefix.empty())
      {
        VideoDecoder decoder(video, outputPrefix);
        if (!decoder.initialize())
        {
          std::cerr << "Failed to initialize decoder\n";
          result->Error("decoder_err", "Failed to initialize decoder\n");
          return;
        }
        else
        {
          decoder.saveFrames();
          result->Success(flutter::EncodableValue("Video decoding completed"));
        }
      }
      else
      {
        result->Error("INVALID_ARGUMENT", "Invalid input or output prefix");
      }
    }
    else
    {
      result->NotImplemented();
    }
  }

} // namespace trimmer

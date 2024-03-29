#pragma warning(disable : 4819)
#pragma warning(disable : 4566)
#include "trimmer_plugin.h"
#include "ffmpeg/VideoDecoder.h"
#include "ffmpeg/VideoDecoderManager.cpp"
#include "utils.cpp"

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
#include <cstdlib>
#include <xtr1common>
#include <cmath>
#include <thread>

#pragma warning(disable : 4244 4819 4996 4505)
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>

#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/parseutils.h>
#include <libavutil/log.h>
#include <libavutil/imgutils.h>
}

using namespace flutter;
using namespace std;

namespace trimmer
{
	// 记录AVFrame的详细信息的函数
	static void log_frame_details(const AVFrame *frame, const std::string &frame_description)
	{
		std::cout << frame_description << std::endl;
		std::cout << "Frame width: " << frame->width << std::endl;
		std::cout << "Frame height: " << frame->height << std::endl;
		std::cout << "Pixel format: " << av_get_pix_fmt_name((AVPixelFormat)frame->format) << std::endl;
		std::cout << "Keyframe: " << (frame->key_frame ? "Yes" : "No") << std::endl;
		std::cout << "Picture type: " << av_get_picture_type_char(frame->pict_type) << std::endl;
		std::cout << "PTS: " << frame->pts << std::endl;
		// 更多的属性可以根据需要记录
	}
	// 记录AVCodecContext的详细信息的函数
	static void log_codec_context_details(const AVCodecContext *ctx, const std::string &frame_description)
	{
		std::cout << frame_description << std::endl;
		std::cout << "Codec ID: " << ctx->codec_id << std::endl;
		std::cout << "Codec Type: " << ctx->codec_type << std::endl;
		std::cout << "Bit Rate: " << ctx->bit_rate << std::endl;
		std::cout << "Width: " << ctx->width << std::endl;
		std::cout << "Height: " << ctx->height << std::endl;
		std::cout << "Time Base: " << ctx->time_base.num << "/" << ctx->time_base.den << std::endl;
		std::cout << "Gop Size: " << ctx->gop_size << std::endl;
		std::cout << "Max B Frames: " << ctx->max_b_frames << std::endl;
		std::cout << "Pixel Format: " << av_get_pix_fmt_name(ctx->pix_fmt) << std::endl;
		// 更多的属性可以根据需要记录
	}
	static bool saveFrameAsPNG(AVFrame *frameRGB, int w, int h, const std::string &outputPath)
	{
		AVCodec *codec;
		AVCodecContext *codecContext;
		AVPacket packet;
		int ret;

		codec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_PNG));
		if (!codec)
		{
			std::cerr << "Failed to find PNG encoder" << std::endl;
			return false;
		}

		codecContext = avcodec_alloc_context3(codec);
		if (!codecContext)
		{
			std::cerr << "Failed to allocate codec context" << std::endl;
			return false;
		}

		// std::cerr << "frameRGB width" << frameRGB->width << std::endl;
		// std::cerr << "frameRGB height" << frameRGB->height << std::endl;

		// codecContext->width = w;
		// codecContext->height = h;

		codecContext->width = frameRGB->width;
		codecContext->height = frameRGB->height;
		codecContext->pix_fmt = AV_PIX_FMT_RGB24;
		// codecContext->time_base = (AVRational){1, 25};
		codecContext->time_base.num = 1;
		codecContext->time_base.den = 1;

		if (avcodec_open2(codecContext, codec, nullptr) < 0)
		{
			std::cerr << "Failed to open PNG encoder" << std::endl;
			return false;
		}

		log_codec_context_details(codecContext, "png codec:");

		// av_packet_alloc
		packet = *av_packet_alloc();
		// av_init_packet(&packet);
		packet.data = nullptr;
		packet.size = 0;

		ret = avcodec_send_frame(codecContext, frameRGB);
		std::cerr << "avcodec_send_frame ret: " << ret << " " << AVERROR(ret) << std::endl;

		if (ret < 0)
		{
			std::cerr << "Error sending frame to encoder" << std::endl;
			return false;
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_packet(codecContext, &packet);
			if (ret < 0)
			{
				std::cerr << "Error during encoding" << std::endl;
				av_packet_unref(&packet);
				return false;
			}

			FILE *outFile;
			fopen_s(&outFile, outputPath.c_str(), "wb");
			if (!outFile)
			{
				std::cerr << "Failed to open output file" << std::endl;
				av_packet_unref(&packet);
				return false;
			}

			fwrite(packet.data, 1, packet.size, outFile);
			fclose(outFile);
			av_packet_unref(&packet);
		}

		avcodec_close(codecContext);
		avcodec_free_context(&codecContext);

		return true;
	}

	static bool extractFrame(const std::string &videoPath, int frameNumber, const std::string &outputPath)
	{
		AVFormatContext *formatContext = nullptr;
		AVCodecContext *codecContext = nullptr;
		AVCodec *codec = nullptr;
		AVFrame *frame = nullptr;
		AVFrame *frameRGB = nullptr;
		AVPacket packet;
		struct SwsContext *swsContext = nullptr;

		if (avformat_open_input(&formatContext, videoPath.c_str(), nullptr, nullptr) < 0)
		{
			std::cerr << "Error opening input file" << std::endl;
			return false;
		}

		if (avformat_find_stream_info(formatContext, nullptr) < 0)
		{
			std::cerr << "Error finding stream info" << std::endl;
			return false;
		}

		int videoStreamIndex = -1;
		for (unsigned int i = 0; i < formatContext->nb_streams; i++)
		{
			if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				videoStreamIndex = i;
				break;
			}
		}

		if (videoStreamIndex == -1)
		{
			std::cerr << "Could not find video stream" << std::endl;
			return false;
		}

		codec = const_cast<AVCodec *>(avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id));
		if (!codec)
		{
			std::cerr << "Unsupported codec" << std::endl;
			return false;
		}

		codecContext = avcodec_alloc_context3(codec);
		if (!codecContext)
		{
			std::cerr << "Failed to allocate codec context" << std::endl;
			return false;
		}

		if (avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar) < 0)
		{
			std::cerr << "Failed to copy codec parameters to codec context" << std::endl;
			return false;
		}

		if (avcodec_open2(codecContext, codec, nullptr) < 0)
		{
			std::cerr << "Failed to open codec" << std::endl;
			return false;
		}

		frame = av_frame_alloc();
		if (!frame)
		{
			std::cerr << "Failed to allocate frame" << std::endl;
			return false;
		}

		frameRGB = av_frame_alloc();
		if (!frameRGB)
		{
			std::cerr << "Failed to allocate RGB frame" << std::endl;
			return false;
		}

		// 通过传入图像的像素格式（pix_fmt）、宽度（width）、高度（height）以及对齐方式（align），来计算所需的缓冲区大小（以字节为单位）
		// 配合av_image_fill_arrays, 填充基本value
		int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
		uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
		av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

		// av_log_set_level(AV_LOG_DEBUG);
		// 格式转换器, 源(宽,高,编码), 目标,
		// SWS_BILINEAR：用于图像缩放的算法，这里指定为双线性插值，
		// nullptr, nullptr, nullptr：这三个参数分别是源图像的调色板、目标图像的调色板和特殊选项，
		swsContext = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt, codecContext->width, codecContext->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

		// log_codec_context_details(codecContext, "codecContext");
		if (!swsContext)
		{
			std::cerr << "Failed to create SwsContext" << std::endl;
			return false;
		}

		// int frameFinished;
		int currentFrame = 0;
		while (av_read_frame(formatContext, &packet) >= 0) // 解出包就继续执行,
		{
			if (packet.stream_index == videoStreamIndex) // 解出包是需要的视频流(videoStreamIndex), 进入下一步帧提取
			{
				avcodec_send_packet(codecContext, &packet);				// 解码器解码这一包
				while (avcodec_receive_frame(codecContext, frame) == 0) // 返回值为0表示成功获取到一帧解码后的数据
				{
					if (currentFrame == frameNumber)
					{

						sws_scale(swsContext, (uint8_t const *const *)frame->data, frame->linesize, 0, codecContext->height, frameRGB->data, frameRGB->linesize);
						log_frame_details(frame, "org");
						// log_frame_details(frameRGB, "convertToRGB"); // 不能log这东西,就是缺信息
						// 设置AVFrame的属性
						frameRGB->width = codecContext->width;
						frameRGB->height = codecContext->height;
						frameRGB->format = AV_PIX_FMT_RGB24;

						AVCodec *pngCodec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_PNG));
						if (!pngCodec)
						{
							std::cerr << "Failed to find PNG encoder" << std::endl;
							return false;
						}
						AVCodecContext *pngCodecCtx = avcodec_alloc_context3(pngCodec);
						if (!pngCodecCtx)
						{
							std::cerr << "Failed to allocate codec context" << std::endl;
							return false;
						}
						pngCodecCtx->width = frameRGB->width;
						pngCodecCtx->height = frameRGB->height;
						pngCodecCtx->pix_fmt = AV_PIX_FMT_RGB24;
						pngCodecCtx->time_base.num = 1;
						pngCodecCtx->time_base.den = 1;
						if (avcodec_open2(pngCodecCtx, pngCodec, nullptr) < 0)
						{
							std::cerr << "打开编码器失败" << std::endl;
							return false;
						}
						FILE *outFile;
						fopen_s(&outFile, outputPath.c_str(), "wb");
						if (!outFile)
						{
							std::cerr << "文件打开失败" << std::endl;
							av_packet_unref(&packet);
							return false;
						}
						// 分配输出数据结构
						AVPacket *pngPkt = av_packet_alloc();
						if (!pngPkt)
						{
							fprintf(stderr, "Error allocating packet(pngPkt)\n");
							return false;
						}
						int pngRet = avcodec_send_frame(pngCodecCtx, frameRGB);
						if (pngRet < 0)
						{
							fprintf(stderr, "Error sending frame for encoding\n");
							return false;
						}
						while (pngRet >= 0)
						{
							pngRet = avcodec_receive_packet(pngCodecCtx, pngPkt);
							if (pngRet == AVERROR(EAGAIN) || pngRet == AVERROR_EOF)
							{
								break;
							}
							else if (pngRet < 0)
							{
								fprintf(stderr, "Error during encoding\n");
								return pngRet;
							}

							fwrite(pngPkt->data, 1, pngPkt->size, outFile);
							av_packet_unref(pngPkt);
						}
						// 释放资源
						fclose(outFile);
						av_packet_free(&pngPkt);
						avcodec_free_context(&pngCodecCtx);

						av_packet_unref(&packet);
						av_frame_unref(frame);
						av_frame_free(&frame);
						av_frame_free(&frameRGB);
						avcodec_close(codecContext);
						avformat_close_input(&formatContext);
						sws_freeContext(swsContext);
						av_free(buffer);
						return true;
					}
					currentFrame++;
				}
			}
			av_packet_unref(&packet); // 清理包, 循环复用
		}

		av_free(buffer);
		av_frame_free(&frameRGB);
		av_frame_free(&frame);
		avcodec_close(codecContext);
		avformat_close_input(&formatContext);
		sws_freeContext(swsContext);

		return false;
	}

	// 在主函数中使用VideoDecoderManager管理VideoDecoder对象
	VideoDecoderManager vdMgr;

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
		else if (method_call.method_name().compare("getTotalFrames") == 0)
		{
			std::string videoPath;
			if (method_call.arguments())
			{
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);
				VideoDecoder *decoder = vdMgr.getDecoder(videoPath);
				// std::cout << "decoder->total_frames: " << decoder->total_frames << std::endl;
				result->Success(EncodableValue(decoder->total_frames));
				return;
			}
			result->Error("INVALID_ARGUMENT", "非法参数");
			return;
		}
		else if (method_call.method_name().compare("getAvgFrameDuration") == 0)
		{
			std::string videoPath;
			if (method_call.arguments())
			{
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);
				VideoDecoder *decoder = vdMgr.getDecoder(videoPath);
				// std::cout << "decoder->total_frames: " << decoder->total_frames << std::endl;
				result->Success(EncodableValue(decoder->avgFrameDuration));
				return;
			}
			result->Error("INVALID_ARGUMENT", "非法参数");
			return;
		}
		else if (method_call.method_name().compare("getDuration") == 0)
		{
			std::string videoPath;
			if (method_call.arguments())
			{
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);
				VideoDecoder *decoder = vdMgr.getDecoder(videoPath);
				std::cout << "decoder->duration: " << decoder->duration << std::endl;
				result->Success(EncodableValue(decoder->duration));
				return;
			}
			result->Error("INVALID_ARGUMENT", "非法参数");
			return;
		}
		else if (method_call.method_name().compare("saveFrameToFile") == 0)
		{
			std::string videoPath;
			int frameIndex = 0;
			std::string frameSavedDirPath;
			if (method_call.arguments())
			{
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);

				const auto &frameIndex_value = arguments_map.find(flutter::EncodableValue("frameIndex"));
				frameIndex = std::get<int>(frameIndex_value->second);

				const auto &frameSavedDirPath_value = arguments_map.find(flutter::EncodableValue("frameSavedDirPath"));
				frameSavedDirPath = std::get<std::string>(frameSavedDirPath_value->second);

				// std::cout << "videoPath: " << videoPath << std::endl;
				// std::cout << "frameIndex: " << frameIndex << std::endl;
				std::cout << "frameSavedDirPath: " << frameSavedDirPath << std::endl;

				VideoDecoder *decoder = vdMgr.getDecoder(videoPath);
				decoder->extractFrame(frameIndex, frameSavedDirPath);
				result->Success(EncodableValue(true));
				return;
			}
			result->Success(EncodableValue(false));
			return;
		}
		else if (method_call.method_name().compare("initVideoDecoder") == 0)
		{
			if (method_call.arguments())
			{
				std::string videoPath;
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);
				vdMgr.getDecoder(videoPath);
				result->Success(EncodableValue(true));
			}
			result->Error("INVALID_Method_args", "videoPath");
			return;
		}
		else if (method_call.method_name().compare("releaseVideoDecoder") == 0)
		{
			if (method_call.arguments())
			{
				std::string videoPath;
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);
				vdMgr.releaseDecoder(videoPath);
				result->Success(EncodableValue(true));
			}
			result->Error("INVALID_Method_args", "videoPath");
			return;
		}
		else if (method_call.method_name().compare("thumbnailPNGData") == 0)
		{
			std::string videoPath;
			int frameIndex = 0;

			if (method_call.arguments())
			{
				const auto &arguments_map = std::get<flutter::EncodableMap>(*method_call.arguments());
				const auto &video_value = arguments_map.find(flutter::EncodableValue("videoPath"));
				videoPath = std::get<std::string>(video_value->second);

				const auto &frameIndex_value = arguments_map.find(flutter::EncodableValue("frameIndex"));
				frameIndex = std::get<int>(frameIndex_value->second);

				std::thread([result = std::move(result), videoPath = std::move(videoPath), frameIndex = std::move(frameIndex)]()
							{
								VideoDecoder *decoder = vdMgr.getDecoder(videoPath);
								std::vector<uint8_t> imageData = decoder->extractFramePNGData(frameIndex);
								result->Success(EncodableValue(imageData)); })
					.detach();
				return;
			}
			result->Error("INVALID_ARGUMENT", "Invalid input or output prefix");
			return;
		}
		else
		{
			result->NotImplemented();
		}
	}

} // namespace trimmer

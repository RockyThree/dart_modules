#pragma warning(disable : 4819)
#include "VideoDecoder.h"
#include <locale>
#include <codecvt>

#pragma warning(disable : 4819 4996)

VideoDecoder::VideoDecoder(const std::string &inputFilePath) : inputFilePath(inputFilePath) {}

bool VideoDecoder::initialize()
{
    // 打开文件, 获取格式ctx
    if (avformat_open_input(&formatContext, inputFilePath.c_str(), nullptr, nullptr) < 0)
    {
        std::cerr << "Error opening input file" << std::endl;
        return false;
    }
    if (avformat_find_stream_info(formatContext, nullptr) < 0)
    {
        std::cerr << "Error finding stream info" << std::endl;
        return false;
    }
    // 根据格式ctx查找视频所在的index
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

    // #region 提取解码器信息, 创建解码器对象
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
    // #endregion

    // 申请frame的内存空间

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
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
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

    return true;
}

VideoDecoder::~VideoDecoder()
{
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_close(codecContext);
    avformat_close_input(&formatContext);
    sws_freeContext(swsContext);
    av_free(buffer);
}

void VideoDecoder::extractFrame(int frameNumber, const std::string &outputPath)
{
    bool isFoundFrame = false;
    // 假设要定位到第10帧
    int64_t frameIndex = frameNumber;

    std::cout << "extractFrame, frameIndex: " << frameIndex << std::endl;

    // AVStream *videoStream = formatContext->streams[videoStreamIndex];
    // int64_t timestamp = av_rescale_q(frameIndex, videoStream->time_base, AV_TIME_BASE_Q); // 根据帧推时间戳
    // std::cerr << "seek to timestamp " << timestamp << std::endl;
    // av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);

    // av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    int ret = av_seek_frame(formatContext, videoStreamIndex, frameIndex, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
    std::cerr << "seek to frameIndex return: " << ret << std::endl;
    // 刷新解码器的缓冲区
    // avcodec_flush_buffers(codecContext);
    avformat_flush(formatContext);
    while (av_read_frame(formatContext, &packet) >= 0 && !isFoundFrame) // 解出包就继续执行,
    {
        if (packet.stream_index == videoStreamIndex) // 解出包是需要的视频流(videoStreamIndex), 进入下一步帧提取
        {
            avcodec_send_packet(codecContext, &packet);                              // 解码器解码这一包
            while (avcodec_receive_frame(codecContext, frame) == 0 && !isFoundFrame) // 返回值为0表示成功获取到一帧解码后的数据
            {
                isFoundFrame = true; // 因为一开始用seek了, 解码出1帧,就是需要的图
                sws_scale(swsContext, (uint8_t const *const *)frame->data, frame->linesize, 0, codecContext->height, frameRGB->data, frameRGB->linesize);
                // 设置AVFrame的属性
                frameRGB->width = codecContext->width;
                frameRGB->height = codecContext->height;
                frameRGB->format = AV_PIX_FMT_RGB24;

                AVCodec *pngCodec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_PNG));
                if (!pngCodec)
                {
                    std::cerr << "Failed to find PNG encoder" << std::endl;
                    break;
                }
                AVCodecContext *pngCodecCtx = avcodec_alloc_context3(pngCodec);
                if (!pngCodecCtx)
                {
                    std::cerr << "Failed to allocate codec context" << std::endl;
                    break;
                }
                pngCodecCtx->width = frameRGB->width;
                pngCodecCtx->height = frameRGB->height;
                pngCodecCtx->pix_fmt = AV_PIX_FMT_RGB24;
                pngCodecCtx->time_base.num = 1;
                pngCodecCtx->time_base.den = 1;
                if (avcodec_open2(pngCodecCtx, pngCodec, nullptr) < 0)
                {
                    std::cerr << "打开编码器失败" << std::endl;
                    break;
                }
                FILE *outFile;
                fopen_s(&outFile, outputPath.c_str(), "wb");
                if (!outFile)
                {
                    std::cerr << "文件打开失败" << std::endl;
                    break;
                }
                // 分配输出数据结构
                AVPacket *pngPkt = av_packet_alloc();
                if (!pngPkt)
                {
                    fprintf(stderr, "Error allocating packet(pngPkt)\n");
                    break;
                }
                int pngRet = avcodec_send_frame(pngCodecCtx, frameRGB);
                if (pngRet < 0)
                {
                    fprintf(stderr, "Error sending frame for encoding\n");
                    break;
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
                        break;
                    }

                    fwrite(pngPkt->data, 1, pngPkt->size, outFile);
                    av_packet_unref(pngPkt);
                }
                // 释放资源
                fclose(outFile);
                av_packet_unref(pngPkt); // 清理包,
                av_packet_free(&pngPkt);
                avcodec_free_context(&pngCodecCtx);
            }
            av_frame_unref(frame);
        }
        av_packet_unref(&packet); // 清理包, 循环复用
    }
}
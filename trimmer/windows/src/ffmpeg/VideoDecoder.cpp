#pragma warning(disable : 4819)
#pragma warning(disable : 4566)
#include "VideoDecoder.h"
#include <locale>
#include <codecvt>
#include "../utils.cpp"

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

    // Convert timebase to ticks so we can easily convert stream's timestamps to ticks
    m_streamTimebase = av_q2d(formatContext->streams[videoStreamIndex]->time_base) * 1000.0 * 10000.0;
    // We will need this when we seek (adding it to seek timestamp);
    startTime = formatContext->streams[videoStreamIndex]->start_time != AV_NOPTS_VALUE ? (long)(formatContext->streams[videoStreamIndex]->start_time * m_streamTimebase) : 0;
    avgFrameDuration = 10000000 / av_q2d(formatContext->streams[videoStreamIndex]->avg_frame_rate);

    if (formatContext->streams[videoStreamIndex]->nb_frames)
    {
        total_frames = formatContext->streams[videoStreamIndex]->nb_frames;
    }
    else
    {
        // total_frames = formatContext->streams[videoStreamIndex]->avg_frame_rate.num * formatContext->streams[videoStreamIndex]->duration /
        //                formatContext->streams[videoStreamIndex]->avg_frame_rate.den /
        //                (formatContext->streams[videoStreamIndex]->time_base.num / formatContext->streams[videoStreamIndex]->time_base.den);

        total_frames = formatContext->streams[videoStreamIndex]->avg_frame_rate.num * formatContext->streams[videoStreamIndex]->duration /
                       formatContext->streams[videoStreamIndex]->avg_frame_rate.den / 1000;
    }

    duration = formatContext->streams[videoStreamIndex]->duration;

    std::cerr << "video duration " << duration << std::endl;
    std::cerr << "video total_frames " << total_frames << std::endl;
    std::cerr << "video avgFrameDuration " << avgFrameDuration << std::endl;
    std::cerr << "video startTime " << startTime << std::endl;

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
    long frameTimestamp = (long)(frameNumber * avgFrameDuration);

    av_seek_frame(formatContext, -1, (startTime + frameTimestamp) / 10, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    // av_seek_frame(formatContext, videoStreamIndex, (startTime + frameTimestamp) / 10, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    // int ret = avformat_seek_file(
    //     formatContext, videoStreamIndex,
    //     INT64_MIN, frameIndex, INT64_MAX,
    //     AVSEEK_FLAG_FRAME | AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecContext);

    while (av_read_frame(formatContext, &packet) >= 0) // 解出包就继续执行,
    {
        if (packet.stream_index == videoStreamIndex) // 解出包是需要的视频流(videoStreamIndex), 进入下一步帧提取
        {
            avcodec_send_packet(codecContext, &packet);                              // 解码器解码这一包
            av_packet_unref(&packet);                                                // 清理包, 循环复用
            while (avcodec_receive_frame(codecContext, frame) == 0 && !isFoundFrame) // 返回值为0表示成功获取到一帧解码后的数据
            {

                // 尽可能地获取帧的时间戳，并在没有有效时间戳时跳过当前帧的处理
                long curPts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? frame->pts : frame->best_effort_timestamp;
                if (curPts == AV_NOPTS_VALUE)
                {
                    av_frame_unref(frame);
                    continue;
                }

                // 跳过那些在我们实际请求的帧之前的帧。
                if ((long)(curPts * m_streamTimebase) / 10000 < frameTimestamp / 10000)
                {
                    av_frame_unref(frame);
                    continue;
                }

                // 找到
                isFoundFrame = true;

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

                av_frame_unref(frame);
            }
        }
        else
        {
            av_packet_unref(&packet); // 清理包, 循环复用
            continue;
        }
    }
}

std::vector<uint8_t> VideoDecoder::extractFramePNGData(int frameNumber)
{
    std::unique_lock<std::mutex> lock(mtx); // 加锁
    std::vector<uint8_t> encodedData;

    bool isFoundFrame = false;
    int64_t frameTimestamp = (int64_t)(frameNumber * avgFrameDuration);
    int64_t ts = (startTime + frameTimestamp) / 10;
    av_seek_frame(formatContext, -1, ts, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);

    // std::cerr << "frameTimestamp " << frameTimestamp << std::endl;
    // std::cerr << "ts " << ts << std::endl;

    // avformat_seek_file(
    //     formatContext, -1,
    //     ts - 1000, ts, ts + 1000,
    //     AVSEEK_FLAG_FRAME | AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecContext);

    while (av_read_frame(formatContext, &packet) >= 0 && !isFoundFrame) // 解出包就继续执行,
    {
        if (packet.stream_index == videoStreamIndex) // 解出包是需要的视频流(videoStreamIndex), 进入下一步帧提取
        {
            avcodec_send_packet(codecContext, &packet);                              // 解码器解码这一包
            av_packet_unref(&packet);                                                // 清理包, 循环复用
            while (avcodec_receive_frame(codecContext, frame) == 0 && !isFoundFrame) // 返回值为0表示成功获取到一帧解码后的数据
            {

                // 尽可能地获取帧的时间戳，并在没有有效时间戳时跳过当前帧的处理
                int64_t curPts = frame->best_effort_timestamp == AV_NOPTS_VALUE ? frame->pts : frame->best_effort_timestamp;
                if (curPts == AV_NOPTS_VALUE)
                {
                    av_frame_unref(frame);
                    continue;
                }

                // 跳过那些在我们实际请求的帧之前的帧。
                if ((int64_t)(curPts * m_streamTimebase) / 10000 < frameTimestamp / 10000)
                {
                    av_frame_unref(frame);
                    continue;
                }

                // 找到
                isFoundFrame = true;

                sws_scale(swsContext, (uint8_t const *const *)frame->data, frame->linesize, 0, codecContext->height, frameRGB->data, frameRGB->linesize);
                // 设置AVFrame的属性
                frameRGB->width = codecContext->width;
                frameRGB->height = codecContext->height;
                frameRGB->format = AV_PIX_FMT_RGB24;

                encodedData = convertFrame(AV_CODEC_ID_PNG);
                // encodedData = convertFrame(AV_CODEC_ID_WEBP);
                av_frame_unref(frame);
            }
        }
        else
        {
            av_packet_unref(&packet); // 清理包, 循环复用
            continue;
        }
    }
    lock.unlock(); // 手动解锁
    return encodedData;
}

std::vector<uint8_t> VideoDecoder::convertFrame(AVCodecID avcid)
{
    std::vector<uint8_t> encodedData;

    AVCodec *convertCodec = const_cast<AVCodec *>(avcodec_find_encoder(avcid));
    if (!convertCodec)
    {
        std::cerr << "convertCodec, Failed to find " << avcid << " encoder" << std::endl;
        return encodedData;
    }
    AVCodecContext *convertCodecCtx = avcodec_alloc_context3(convertCodec);
    if (!convertCodecCtx)
    {
        std::cerr << "convertCodecCtx, Failed to allocate codec context" << std::endl;
        return encodedData;
    }
    convertCodecCtx->width = frameRGB->width;
    convertCodecCtx->height = frameRGB->height;
    convertCodecCtx->pix_fmt = AV_PIX_FMT_RGB24;
    convertCodecCtx->time_base.num = 1;
    convertCodecCtx->time_base.den = 1;
    if (avcodec_open2(convertCodecCtx, convertCodec, nullptr) < 0)
    {
        std::cerr << "convertFrame: 打开编码器失败" << std::endl;
        return encodedData;
    }

    // 准备输出数据结构
    AVPacket *tmpPkt = av_packet_alloc();
    if (!tmpPkt)
    {
        fprintf(stderr, "Error allocating packet(convertFrame tmpPkt)\n");
        return encodedData;
    }

    // 循环进行编码
    int ret = avcodec_send_frame(convertCodecCtx, frameRGB);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame for encoding\n");
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(convertCodecCtx, tmpPkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error during encoding\n");
            break;
        }

        // 将编码后的数据存储到std::vector<uint8_t>中
        // gpt: pngPkt->data + pngPkt->size这个表达式是一个指针算术运算，它的作用是计算指针偏移量。
        encodedData.insert(encodedData.end(), tmpPkt->data, tmpPkt->data + tmpPkt->size);

        av_packet_unref(tmpPkt);
    }

    // 释放资源
    av_packet_free(&tmpPkt);
    avcodec_free_context(&convertCodecCtx);

    return encodedData;
}
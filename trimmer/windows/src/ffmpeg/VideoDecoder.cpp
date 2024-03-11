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
}

void VideoDecoder::extractFrame(int frameNumber, const std::string &outputPath)
{
    // 假设要定位到第10帧
    int64_t frameIndex = frameNumber;
    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    int64_t timestamp = av_rescale_q(frameIndex, videoStream->time_base, AV_TIME_BASE_Q);
    av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
}
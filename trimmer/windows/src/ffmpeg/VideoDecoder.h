#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#pragma warning(disable : 4244 4267)
#pragma warning(disable : 4819)
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

#define INBUF_SIZE 4096

class VideoDecoder
{
public:
    VideoDecoder(const std::string &inputFilePath);
    bool initialize();
    void extractFrame(int frameNumber, const std::string &outputPath);
    void extractFramePNGData(int frameNumber);
    ~VideoDecoder();

private:
    const std::string inputFilePath;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr; // 解码器上下文
    AVCodec *codec = nullptr;    // 解码器

    uint8_t *buffer;
    AVFrame *frame = nullptr;    // 提取到的帧
    AVFrame *frameRGB = nullptr; // 转码后的帧数据
    AVPacket packet;
    struct SwsContext *swsContext = nullptr;

    // 视频所在
    int videoStreamIndex = -1;
};

#endif // VIDEO_DECODER_H

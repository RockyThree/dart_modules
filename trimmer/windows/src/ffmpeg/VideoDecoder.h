#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>


#pragma warning(disable : 4244 4267)
#pragma warning(disable : 4819)
extern "C" {
#include <libavcodec/avcodec.h>
}

#define INBUF_SIZE 4096

class VideoDecoder {
public:
    VideoDecoder(const std::string& inputFilename, const std::string& outputPrefix);
    bool initialize();
    void saveFrames();
    ~VideoDecoder();

private:
    const std::string inputFilename;
    const std::string outputPrefix;
    std::ifstream inputFile;
    // std::wifstream inputFile;
    const AVCodec *codec = nullptr;
    AVCodecParserContext *parser = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVFrame *frame = nullptr;
    int frameCount = 0;

    void decodeFrame(AVPacket *pkt);
    void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, const char *filename);
};

#endif // VIDEO_DECODER_H

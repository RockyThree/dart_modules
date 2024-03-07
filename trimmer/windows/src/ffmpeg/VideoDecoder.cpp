#include "VideoDecoder.h"
#pragma warning(disable : 4819)

VideoDecoder::VideoDecoder(const std::string &inputFilename, const std::string &outputPrefix) : inputFilename(inputFilename), outputPrefix(outputPrefix) {}

bool VideoDecoder::initialize()
{
    // Find the MPEG-1 video decoder
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec)
    {
        std::cerr << "Codec not found\n";
        return false;
    }

    parser = av_parser_init(codec->id);
    if (!parser)
    {
        std::cerr << "Parser not found\n";
        return false;
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
    {
        std::cerr << "Could not allocate video codec context\n";
        return false;
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        std::cerr << "Could not open codec\n";
        return false;
    }

    // inputFile(inputFilename, std::ios::binary); // 使用wifstream打开文件
    // inputFile.open(inputFilename, std::ios::binary);
    std::ifstream file(inputFilename, std::ios::binary | std::ios::ate);
    // inputFile.open(inputFilename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Could not open " << inputFilename << "\n";
        return false;
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        std::cerr << "Could not allocate video frame\n";
        return false;
    }

    return true;
}

void VideoDecoder::saveFrames()
{
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;
    int ret;
    int eof;
    AVPacket *pkt = av_packet_alloc();

    if (!pkt)
    {
        std::cerr << "Error allocating packet\n";
        return;
    }

    /* Set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    do
    {

        std::ifstream file(inputFilename, std::ios::binary | std::ios::ate);
        // Read raw data from the input file
        file.read(reinterpret_cast<char *>(inbuf), INBUF_SIZE);
        // inputFile.read(reinterpret_cast<wchar_t*>(inbuf), INBUF_SIZE);
        data_size = file.gcount();
        eof = file.eof();

        // Use the parser to split the data into frames
        data = inbuf;
        while (data_size > 0 || eof)
        {
            ret = av_parser_parse2(parser, codecContext, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0)
            {
                std::cerr << "Error while parsing\n";
                return;
            }
            data += ret;
            data_size -= ret;

            if (pkt->size)
                decodeFrame(pkt);
            else if (eof)
                break;
        }
    } while (!eof);

    // Flush the decoder
    decodeFrame(nullptr);

    av_packet_free(&pkt);
}

VideoDecoder::~VideoDecoder()
{
    av_parser_close(parser);
    avcodec_free_context(&codecContext);
    av_frame_free(&frame);
    inputFile.close();
}

void VideoDecoder::decodeFrame(AVPacket *pkt)
{
    char buf[1024];
    int ret;

    ret = avcodec_send_packet(codecContext, pkt);
    if (ret < 0)
    {
        std::cerr << "Error sending a packet for decoding\n";
        return;
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            std::cerr << "Error during decoding\n";
            return;
        }

        std::string filename = outputPrefix + std::to_string(frameCount++);
        snprintf(buf, sizeof(buf), "%s.pgm", filename.c_str());
        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
    }
}

void VideoDecoder::pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, const char *filename)
{
    std::ofstream f(filename, std::ios::binary);
    if (!f)
    {
        std::cerr << "Error opening file " << filename << " for writing\n";
        return;
    }
    f << "P5\n"
      << xsize << " " << ysize << "\n"
      << 255 << "\n";
    for (int i = 0; i < ysize; i++)
        f.write(reinterpret_cast<char *>(buf + i * wrap), xsize);
}

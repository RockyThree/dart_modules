
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

// 将AVFrame转换为PNG格式并保存到文件的函数
int save_frame_to_png(AVFrame *frame, const char *filename) {
    int ret;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVPacket pkt;
    uint8_t *out_buffer = NULL;

    // 注册所有的编解码器
    avcodec_register_all();

    // 查找PNG编码器
    pCodec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!pCodec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    // 创建编码器上下文
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    // 设置编码器上下文参数
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->width = frame->width;
    pCodecCtx->height = frame->height;
    pCodecCtx->pix_fmt = AV_PIX_FMT_RGBA;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;

    // 打开编码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    // 分配输出缓冲区
    out_buffer = av_malloc(av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 1));
    av_image_fill_arrays(frame->data, frame->linesize, out_buffer, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);

    // 初始化AVPacket
    av_init_packet(&pkt);
    pkt.data = NULL;    // packet data will be allocated by the encoder
    pkt.size = 0;

    // 编码一帧图像
    ret = avcodec_send_frame(pCodecCtx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return -1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(pCodecCtx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return -1;
        }

        // 将编码后的数据写入文件
        FILE *file = fopen(filename, "wb");
        if (!file) {
            fprintf(stderr, "Could not open %s\n", filename);
            return -1;
        }
        fwrite(pkt.data, 1, pkt.size, file);
        fclose(file);

        av_packet_unref(&pkt);
    }

    // 清理
    if (out_buffer) {
        av_free(out_buffer);
    }
    avcodec_close(pCodecCtx);
    avcodec_free_context(&pCodecCtx);

    return 0;
}

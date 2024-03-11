#pragma warning(disable : 4244 4819 26495)
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
class FrameExtractor
{
public:
	FrameExtractor() : pFormatCtx(nullptr), pCodecCtx(nullptr), pFrame(nullptr), pFrameRGB(nullptr), buffer(nullptr) {}

	~FrameExtractor()
	{
		if (pFrameRGB)
			av_free(pFrameRGB);
		if (pFrame)
			av_free(pFrame);
		if (buffer)
			av_free(buffer);
		if (pCodecCtx)
			avcodec_close(pCodecCtx);
		if (pFormatCtx)
			avformat_close_input(&pFormatCtx);
	}

	bool open(const char* filename)
	{

		// Open video file
		if (avformat_open_input(&pFormatCtx, filename, nullptr, nullptr) != 0)
		{
			return false; // Couldn't open file
		}

		// Retrieve stream information
		if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
		{
			return false; // Couldn't find stream information
		}

		// Find the first video stream
		videoStream = -1;
		for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
		{
			if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				videoStream = i;
				break;
			}
		}
		if (videoStream == -1)
		{
			return false; // Didn't find a video stream
		}

		// Get a pointer to the codec context for the video stream
		pCodecCtx = avcodec_alloc_context3(nullptr);
		if (!pCodecCtx)
		{
			return false; // Could not allocate AVCodecContext
		}

		// Copy codec context
		if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar) < 0)
		{
			return false; // Couldn't copy codec context
		}

		// Find the decoder for the video stream
		pCodec = const_cast<AVCodec*>(avcodec_find_decoder(pCodecCtx->codec_id));
		if (pCodec == nullptr)
		{
			return false; // Codec not found
		}

		// Open codec
		if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
		{
			return false; // Could not open codec
		}

		// Allocate video frame
		pFrame = av_frame_alloc();

		// Allocate an AVFrame structure
		pFrameRGB = av_frame_alloc();
		if (pFrameRGB == nullptr)
			return false;

		// Determine required buffer size and allocate buffer
		numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
			pCodecCtx->height, 32);
		buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

		av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24,
			pCodecCtx->width, pCodecCtx->height, 32);

		// initialize SWS context for software scaling
		sws_ctx = sws_getContext(pCodecCtx->width,
			pCodecCtx->height,
			pCodecCtx->pix_fmt,
			pCodecCtx->width,
			pCodecCtx->height,
			AV_PIX_FMT_RGB24,
			SWS_BILINEAR,
			nullptr,
			nullptr,
			nullptr);

		return true;
	}

	bool saveFrame(int frameNumber1, const char* outFilename1)
	{
		// Read frames and save first five frames to disk
		int i = 0;
		while (av_read_frame(pFormatCtx, &packet) >= 0)
		{
			// Is this a packet from the video stream?
			if (packet.stream_index == videoStream)
			{
				avcodec_send_packet(pCodecCtx, &packet);

				int ret = avcodec_receive_frame(pCodecCtx, pFrame);
				if (ret >= 0)
				{
					// Convert the image from its native format to RGB

					sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
							  pFrame->linesize, 0, pCodecCtx->height,
							  pFrameRGB->data, pFrameRGB->linesize);

							  // Save the frame to disk
					if (++i == frameNumber1)
					{
						// Save the frame to disk
						 save_image(pFrameRGB, pCodecCtx->width, pCodecCtx->height, outFilename1);
						//save_frame_as_png(pFrame, outFilename1);
						break;
					}
				}
				else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
				}
				else
				{
				}
			}

			// Free the packet that was allocated by av_read_frame
			av_packet_unref(&packet);
		}

		return true;
	}


private:
	AVFormatContext* pFormatCtx;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame;
	AVFrame* pFrameRGB;
	AVPacket packet;
	int videoStream;
	int frameFinished;
	int numBytes;
	uint8_t* buffer;
	struct SwsContext* sws_ctx;

	void save_image(AVFrame* pFrame1, int width, int height, const char* outFilename1)
	{
		FILE* pFile;
		errno_t err = fopen_s(&pFile, outFilename1, "wb");
		if (err != 0)
		{
			return;
		}
		if (pFile == nullptr)
			return;

		// Write header
		fprintf(pFile, "P6\n%d %d\n255\n", width, height);

		// Write pixel data
		for (int y = 0; y < height; y++)
			fwrite(pFrame1->data[0] + y * pFrame1->linesize[0], 1, width * 3, pFile);

		// Close file
		fclose(pFile);
	}

	void save_frame_as_png(AVFrame* pFrameRGB1, const char* outFilename1)
	{
		AVCodec* pCodec1;
		AVCodecContext* pCodecCtx1 = NULL;
		AVPacket* pkt1;
		int ret;

		// Find the PNG codec
		//pCodec1 = const_cast<AVCodec*>(avcodec_find_encoder(AV_CODEC_ID_PNG));
		pCodec1 = const_cast<AVCodec*>(avcodec_find_encoder(AV_CODEC_ID_H264));
		if (!pCodec1)
		{
			fprintf(stderr, "Codec not found\n");
			exit(1);
		}

		pCodecCtx1 = avcodec_alloc_context3(pCodec1);
		if (!pCodecCtx1)
		{
			fprintf(stderr, "Could not allocate video codec context\n");
			exit(1);
		}

		// Set context settings
		pCodecCtx1->width = pFrameRGB1->width;
		pCodecCtx1->height = pFrameRGB1->height;
		//pCodecCtx1->pix_fmt = AV_PIX_FMT_RGB24;
		pCodecCtx1->pix_fmt = AV_PIX_FMT_YUV420P;
		pCodecCtx1->codec_type = AVMEDIA_TYPE_VIDEO;
		pCodecCtx1->time_base.num = 1;
		pCodecCtx1->time_base.den = 25;

		// Open codec
		if (avcodec_open2(pCodecCtx1, pCodec1, NULL) < 0)
		{
			fprintf(stderr, "Could not open codec\n");
			exit(1);
		}

		// Allocate AVPacket
		pkt1 = av_packet_alloc();
		pkt1->data = NULL; // packet data will be allocated by the encoder
		pkt1->size = 0;

		// Encode the image
		ret = avcodec_send_frame(pCodecCtx1, pFrameRGB1);
		if (ret < 0)
		{
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		ret = avcodec_receive_packet(pCodecCtx1, pkt1);
		if (ret < 0)
		{
			fprintf(stderr, "Error receiving packet\n");
			exit(1);
		}

		// Save the encoded frame to file
		FILE* outputFile;
		fopen_s(&outputFile, outFilename1, "wb");
		if (!outputFile)
		{
			fprintf(stderr, "Could not open output file\n");
			exit(1);
		}

		fwrite(pkt1->data, 1, pkt1->size, outputFile);
		fclose(outputFile);

		// Clean up
		av_packet_unref(pkt1);
		avcodec_free_context(&pCodecCtx1);
	}
};

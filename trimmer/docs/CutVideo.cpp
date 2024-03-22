extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
}

/**
 * @brief Print the information of the passed packet.
 *
 * @fn logPacket
 * @param avFormatContext AVFormatContext of the given packet.
 * @param avPacket AVPacket to log.
 * @param tag String to tag the log output.
 */
void logPacket(const AVFormatContext *avFormatContext, const AVPacket *avPacket, const QString tag) {
  AVRational *timeBase = &avFormatContext->streams[avPacket->stream_index]->time_base;

  qDebug() << QString("%1: pts:%2 pts_time:%3 dts:%4 dts_time:%5 duration:%6 duration_time:%7 stream_index:%8")
                  .arg(tag)
                  .arg(av_ts2str(avPacket->pts))
                  .arg(av_ts2timestr(avPacket->pts, timeBase))
                  .arg(av_ts2str(avPacket->dts))
                  .arg(av_ts2timestr(avPacket->dts, timeBase))
                  .arg(av_ts2str(avPacket->duration))
                  .arg(av_ts2timestr(avPacket->duration, timeBase))
                  .arg(avPacket->stream_index);
}

/**
 * @brief Cut a file in the given input file path based on the start and end seconds, and output the cutted file to the
 * given output file path.
 *
 * @fn cutFile
 * @param inputFilePath Input file path to be cutted.
 * @param startSeconds Cutting start time in seconds.
 * @param endSeconds Cutting end time in seconds.
 * @param outputFilePath Output file path to write the new cutted file.
 *
 * @details This function will take an input file path and cut it based on the given start and end seconds. The cutted
 * file will then be outputted to the given output file path.
 *
 * @return True if the cutting operation finished successfully, false otherwise.
 */
bool cutFile(const QString& inputFilePath, const long long& startSeconds, const long long& endSeconds,
                            const QString& outputFilePath) {
  int operationResult;

  AVPacket* avPacket = NULL;
  AVFormatContext* avInputFormatContext = NULL;
  AVFormatContext* avOutputFormatContext = NULL;

  avPacket = av_packet_alloc();
  if (!avPacket) {
    qCritical("Failed to allocate AVPacket.");
    return false;
  }

  try {
    operationResult = avformat_open_input(&avInputFormatContext, inputFilePath.toStdString().c_str(), 0, 0);
    if (operationResult < 0) {
      throw std::runtime_error(QString("Failed to open the input file '%1'.").arg(inputFilePath).toStdString().c_str());
    }

    operationResult = avformat_find_stream_info(avInputFormatContext, 0);
    if (operationResult < 0) {
      throw std::runtime_error(QString("Failed to retrieve the input stream information.").toStdString().c_str());
    }

    avformat_alloc_output_context2(&avOutputFormatContext, NULL, NULL, outputFilePath.toStdString().c_str());
    if (!avOutputFormatContext) {
      operationResult = AVERROR_UNKNOWN;
      throw std::runtime_error(QString("Failed to create the output context.").toStdString().c_str());
    }

    int streamIndex = 0;
    int streamMapping[avInputFormatContext->nb_streams];
    int streamRescaledStartSeconds[avInputFormatContext->nb_streams];
    int streamRescaledEndSeconds[avInputFormatContext->nb_streams];

    // Copy streams from the input file to the output file.
    for (int i = 0; i < avInputFormatContext->nb_streams; i++) {
      AVStream* outStream;
      AVStream* inStream = avInputFormatContext->streams[i];

      streamRescaledStartSeconds[i] = av_rescale_q(startSeconds * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);
      streamRescaledEndSeconds[i] = av_rescale_q(endSeconds * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);

      if (inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
          inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
          inStream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
        streamMapping[i] = -1;
        continue;
      }

      streamMapping[i] = streamIndex++;

      outStream = avformat_new_stream(avOutputFormatContext, NULL);
      if (!outStream) {
        operationResult = AVERROR_UNKNOWN;
        throw std::runtime_error(QString("Failed to allocate the output stream.").toStdString().c_str());
      }

      operationResult = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
      if (operationResult < 0) {
        throw std::runtime_error(
            QString("Failed to copy codec parameters from input stream to output stream.").toStdString().c_str());
      }
      outStream->codecpar->codec_tag = 0;
    }

    if (!(avOutputFormatContext->oformat->flags & AVFMT_NOFILE)) {
      operationResult = avio_open(&avOutputFormatContext->pb, outputFilePath.toStdString().c_str(), AVIO_FLAG_WRITE);
      if (operationResult < 0) {
        throw std::runtime_error(
            QString("Failed to open the output file '%1'.").arg(outputFilePath).toStdString().c_str());
      }
    }

    operationResult = avformat_write_header(avOutputFormatContext, NULL);
    if (operationResult < 0) {
      throw std::runtime_error(QString("Error occurred when opening output file.").toStdString().c_str());
    }

    operationResult = avformat_seek_file(avInputFormatContext, -1, INT64_MIN, startSeconds * AV_TIME_BASE,
                                         startSeconds * AV_TIME_BASE, 0);
    if (operationResult < 0) {
      throw std::runtime_error(
          QString("Failed to seek the input file to the targeted start position.").toStdString().c_str());
    }

    while (true) {
      operationResult = av_read_frame(avInputFormatContext, avPacket);
      if (operationResult < 0) break;

      // Skip packets from unknown streams and packets after the end cut position.
      if (avPacket->stream_index >= avInputFormatContext->nb_streams || streamMapping[avPacket->stream_index] < 0 ||
          avPacket->pts > streamRescaledEndSeconds[avPacket->stream_index]) {
        av_packet_unref(avPacket);
        continue;
      }

      avPacket->stream_index = streamMapping[avPacket->stream_index];
      logPacket(avInputFormatContext, avPacket, "in");

      // Shift the packet to its new position by subtracting the rescaled start seconds.
      avPacket->pts -= streamRescaledStartSeconds[avPacket->stream_index];
      avPacket->dts -= streamRescaledStartSeconds[avPacket->stream_index];

      av_packet_rescale_ts(avPacket, avInputFormatContext->streams[avPacket->stream_index]->time_base,
                           avOutputFormatContext->streams[avPacket->stream_index]->time_base);
      avPacket->pos = -1;
      logPacket(avOutputFormatContext, avPacket, "out");

      operationResult = av_interleaved_write_frame(avOutputFormatContext, avPacket);
      if (operationResult < 0) {
        throw std::runtime_error(QString("Failed to mux the packet.").toStdString().c_str());
      }
    }

    av_write_trailer(avOutputFormatContext);
  } catch (std::runtime_error e) {
    qCritical("%s", e.what());
  }

  av_packet_free(&avPacket);

  avformat_close_input(&avInputFormatContext);

  if (avOutputFormatContext && !(avOutputFormatContext->oformat->flags & AVFMT_NOFILE))
    avio_closep(&avOutputFormatContext->pb);
  avformat_free_context(avOutputFormatContext);

  if (operationResult < 0 && operationResult != AVERROR_EOF) {
    qCritical("%s", QString("Error occurred: %1.").arg(av_err2str(operationResult)).toStdString().c_str());
    return false;
  }

  return true;
}
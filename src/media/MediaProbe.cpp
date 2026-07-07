#include "nova/media/MediaProbe.h"

#include <QFileInfo>
#include <QImageReader>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace nova::media {

FileMetadata MediaProbe::probe(const std::string& path) {
    FileMetadata meta;
    meta.path = path;
    meta.fileName = QFileInfo(QString::fromStdString(path)).fileName().toStdString();
    meta.fileSizeBytes = QFileInfo(QString::fromStdString(path)).size();

    QImageReader reader(QString::fromStdString(path));
    if (reader.canRead()) {
        meta.isImage = true;
        meta.hasVideo = true;
        meta.formatName = reader.format().toStdString();
        const QSize size = reader.size();
        meta.width = size.width();
        meta.height = size.height();
        meta.durationSeconds = 5.0; // still images default to 5s on timeline
        meta.frameRate = 30.0;
        return meta;
    }

    AVFormatContext* format = nullptr;
    if (avformat_open_input(&format, path.c_str(), nullptr, nullptr) < 0) {
        meta.formatName = "unknown";
        return meta;
    }

    avformat_find_stream_info(format, nullptr);
    meta.formatName = format->iformat && format->iformat->name ? format->iformat->name : "unknown";
    if (format->duration > 0) {
        meta.durationSeconds = static_cast<double>(format->duration) / AV_TIME_BASE;
    }

    for (unsigned i = 0; i < format->nb_streams; ++i) {
        AVStream* stream = format->streams[i];
        const AVCodecParameters* par = stream->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && !meta.hasVideo) {
            meta.hasVideo = true;
            meta.width = par->width;
            meta.height = par->height;
            if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
                meta.frameRate = av_q2d(stream->avg_frame_rate);
            } else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
                meta.frameRate = av_q2d(stream->r_frame_rate);
            }
            const AVCodec* codec = avcodec_find_decoder(par->codec_id);
            if (codec) meta.videoCodec = codec->name;
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && !meta.hasAudio) {
            meta.hasAudio = true;
            const AVCodec* codec = avcodec_find_decoder(par->codec_id);
            if (codec) meta.audioCodec = codec->name;
        }
    }

    avformat_close_input(&format);
    return meta;
}

} // namespace nova::media

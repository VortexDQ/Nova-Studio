#include "nova/media/VideoExporter.h"
#include "nova/core/Logger.h"

#include <array>
#include <cmath>
#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

namespace nova::media {

namespace {
constexpr const char* kModule = "media.VideoExporter";

std::string avError(int code) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    av_strerror(code, buffer.data(), buffer.size());
    return buffer.data();
}

const char* muxerFor(ExportFormat format) {
    switch (format) {
    case ExportFormat::Mp4: return "mp4";
    case ExportFormat::Mov: return "mov";
    case ExportFormat::Mkv: return "matroska";
    case ExportFormat::Webm: return "webm";
    case ExportFormat::Avi: return "avi";
    }
    return "mp4";
}
} // namespace

std::string VideoExporter::extensionFor(ExportFormat format) {
    switch (format) {
    case ExportFormat::Mp4: return ".mp4";
    case ExportFormat::Mov: return ".mov";
    case ExportFormat::Mkv: return ".mkv";
    case ExportFormat::Webm: return ".webm";
    case ExportFormat::Avi: return ".avi";
    }
    return ".mp4";
}

bool VideoExporter::exportClip(const std::string& inputPath,
                               const std::string& outputPath,
                               ExportFormat format,
                               double startSeconds,
                               double endSeconds,
                               std::string* error) {
    if (endSeconds <= startSeconds) {
        if (error) *error = "Invalid export range.";
        return false;
    }

    const auto parent = std::filesystem::path(outputPath).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    AVFormatContext* inFmt = nullptr;
    int ret = avformat_open_input(&inFmt, inputPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        if (error) *error = "Failed to open input: " + avError(ret);
        return false;
    }
    ret = avformat_find_stream_info(inFmt, nullptr);
    if (ret < 0) {
        if (error) *error = "Failed to read stream info: " + avError(ret);
        avformat_close_input(&inFmt);
        return false;
    }

    AVFormatContext* outFmt = nullptr;
    ret = avformat_alloc_output_context2(&outFmt, nullptr, muxerFor(format), outputPath.c_str());
    if (ret < 0 || !outFmt) {
        if (error) *error = "Failed to create output format: " + avError(ret);
        avformat_close_input(&inFmt);
        return false;
    }

    std::vector<int> streamMap(inFmt->nb_streams, -1);
    for (unsigned i = 0; i < inFmt->nb_streams; ++i) {
        AVStream* inStream = inFmt->streams[i];
        if (inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO
            && inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        AVStream* outStream = avformat_new_stream(outFmt, nullptr);
        if (!outStream) {
            if (error) *error = "Failed to create output stream.";
            avformat_free_context(outFmt);
            avformat_close_input(&inFmt);
            return false;
        }
        ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        if (ret < 0) {
            if (error) *error = "Failed to copy stream parameters: " + avError(ret);
            avformat_free_context(outFmt);
            avformat_close_input(&inFmt);
            return false;
        }
        outStream->codecpar->codec_tag = 0;
        streamMap[i] = static_cast<int>(outStream->index);
    }

    if (!(outFmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFmt->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            if (error) *error = "Failed to open output file: " + avError(ret);
            avformat_free_context(outFmt);
            avformat_close_input(&inFmt);
            return false;
        }
    }

    ret = avformat_write_header(outFmt, nullptr);
    if (ret < 0) {
        if (error) *error = "Failed to write header: " + avError(ret);
        if (!(outFmt->oformat->flags & AVFMT_NOFILE)) avio_closep(&outFmt->pb);
        avformat_free_context(outFmt);
        avformat_close_input(&inFmt);
        return false;
    }

    const int64_t startTs = static_cast<int64_t>(startSeconds * AV_TIME_BASE);
    av_seek_frame(inFmt, -1, startTs, AVSEEK_FLAG_BACKWARD);

    const int64_t endTs = static_cast<int64_t>(endSeconds * AV_TIME_BASE);
    AVPacket* packet = av_packet_alloc();
    bool ok = true;

    while (av_read_frame(inFmt, packet) >= 0) {
        AVStream* inStream = inFmt->streams[packet->stream_index];
        const int outIndex = streamMap[packet->stream_index];
        if (outIndex < 0) {
            av_packet_unref(packet);
            continue;
        }

        int64_t pts = packet->pts == AV_NOPTS_VALUE ? packet->dts : packet->pts;
        if (pts != AV_NOPTS_VALUE) {
            const int64_t absUs = av_rescale_q(pts, inStream->time_base, AVRational{1, AV_TIME_BASE});
            if (absUs > endTs) {
                av_packet_unref(packet);
                break;
            }
            if (absUs < startTs) {
                av_packet_unref(packet);
                continue;
            }
        }

        AVStream* outStream = outFmt->streams[outIndex];
        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);
        packet->stream_index = outIndex;
        packet->pos = -1;

        ret = av_interleaved_write_frame(outFmt, packet);
        av_packet_unref(packet);
        if (ret < 0) {
            if (error) *error = "Write failed: " + avError(ret);
            ok = false;
            break;
        }
    }

    av_packet_free(&packet);
    av_write_trailer(outFmt);
    if (!(outFmt->oformat->flags & AVFMT_NOFILE)) avio_closep(&outFmt->pb);
    avformat_free_context(outFmt);
    avformat_close_input(&inFmt);

    if (ok) {
        NOVA_LOG_INFO(kModule, "Exported video to " + outputPath);
    }
    return ok;
}

} // namespace nova::media

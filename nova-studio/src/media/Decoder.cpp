#include "nova/media/Decoder.h"
#include "nova/core/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace nova::media {

namespace {
constexpr const char* kModule = "media.Decoder";
}

Decoder::Decoder() = default;

Decoder::~Decoder() { close(); }

Decoder::Decoder(Decoder&& other) noexcept { *this = std::move(other); }

Decoder& Decoder::operator=(Decoder&& other) noexcept {
    if (this != &other) {
        close();
        formatCtx_ = other.formatCtx_;
        codecCtx_ = other.codecCtx_;
        swsCtx_ = other.swsCtx_;
        videoStreamIndex_ = other.videoStreamIndex_;
        info_ = other.info_;
        lastError_ = std::move(other.lastError_);

        other.formatCtx_ = nullptr;
        other.codecCtx_ = nullptr;
        other.swsCtx_ = nullptr;
        other.videoStreamIndex_ = -1;
    }
    return *this;
}

void Decoder::setError(const std::string& message) {
    lastError_ = message;
    NOVA_LOG_ERROR(kModule, message);
}

bool Decoder::open(const std::string& path) {
    close();

    if (avformat_open_input(&formatCtx_, path.c_str(), nullptr, nullptr) < 0) {
        setError("Failed to open input: " + path);
        return false;
    }

    if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
        setError("Failed to find stream info in: " + path);
        close();
        return false;
    }

    videoStreamIndex_ = av_find_best_stream(formatCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex_ < 0) {
        setError("No video stream found in: " + path);
        close();
        return false;
    }

    AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        setError("No decoder available for codec in: " + path);
        close();
        return false;
    }

    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_ || avcodec_parameters_to_context(codecCtx_, stream->codecpar) < 0) {
        setError("Failed to allocate/populate codec context for: " + path);
        close();
        return false;
    }

    // Allow multi-threaded decode inside FFmpeg itself; the playback engine
    // above this also runs decode on a background thread, but FFmpeg's own
    // internal frame-level threading gives another speedup for intra-heavy
    // codecs like ProRes/DNxHD.
    codecCtx_->thread_count = 0; // auto-detect
    codecCtx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
        setError("Failed to open codec for: " + path);
        close();
        return false;
    }

    info_.width = codecCtx_->width;
    info_.height = codecCtx_->height;
    info_.hasVideo = true;
    info_.hasAudio = av_find_best_stream(formatCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) >= 0;

    AVRational fr = av_guess_frame_rate(formatCtx_, stream, nullptr);
    info_.frameRate = (fr.num > 0 && fr.den > 0) ? av_q2d(fr) : 30.0;

    if (formatCtx_->duration > 0) {
        info_.durationSeconds = static_cast<double>(formatCtx_->duration) / AV_TIME_BASE;
    } else if (stream->duration > 0) {
        info_.durationSeconds = stream->duration * av_q2d(stream->time_base);
    }

    swsCtx_ = sws_getContext(codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt,
                              codecCtx_->width, codecCtx_->height, AV_PIX_FMT_RGBA,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx_) {
        setError("Failed to create scaling context for: " + path);
        close();
        return false;
    }

    NOVA_LOG_INFO(kModule, "Opened " + path + " (" + std::to_string(info_.width) + "x" +
                                std::to_string(info_.height) + " @ " +
                                std::to_string(info_.frameRate) + "fps)");
    return true;
}

void Decoder::close() {
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
    }
    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
    }
    videoStreamIndex_ = -1;
    info_ = MediaInfo{};
}

bool Decoder::isOpen() const { return formatCtx_ != nullptr && codecCtx_ != nullptr; }

bool Decoder::decodeNextPacket(AVFrame* frame) {
    AVPacket* packet = av_packet_alloc();
    bool gotFrame = false;

    while (!gotFrame && av_read_frame(formatCtx_, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex_) {
            if (avcodec_send_packet(codecCtx_, packet) >= 0) {
                int ret = avcodec_receive_frame(codecCtx_, frame);
                if (ret >= 0) {
                    gotFrame = true;
                }
                // AVERROR(EAGAIN) just means "send more packets"; loop continues.
            }
        }
        av_packet_unref(packet);
    }

    // Flush the decoder at end-of-stream so buffered frames are still returned.
    if (!gotFrame) {
        avcodec_send_packet(codecCtx_, nullptr);
        gotFrame = avcodec_receive_frame(codecCtx_, frame) >= 0;
    }

    av_packet_free(&packet);
    return gotFrame;
}

std::optional<VideoFrame> Decoder::nextFrame() {
    if (!isOpen()) return std::nullopt;

    AVFrame* frame = av_frame_alloc();
    if (!decodeNextPacket(frame)) {
        av_frame_free(&frame);
        return std::nullopt;
    }

    VideoFrame out;
    out.width = codecCtx_->width;
    out.height = codecCtx_->height;
    out.pts = frame->pts;

    AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    out.timeSeconds = frame->pts * av_q2d(stream->time_base);

    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    uint8_t* dstData[4] = {out.rgba.data(), nullptr, nullptr, nullptr};
    int dstLinesize[4] = {out.width * 4, 0, 0, 0};

    sws_scale(swsCtx_, frame->data, frame->linesize, 0, out.height, dstData, dstLinesize);

    av_frame_free(&frame);
    return out;
}

bool Decoder::seek(double seconds) {
    if (!isOpen()) return false;
    AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    int64_t target = static_cast<int64_t>(seconds / av_q2d(stream->time_base));

    if (av_seek_frame(formatCtx_, videoStreamIndex_, target, AVSEEK_FLAG_BACKWARD) < 0) {
        setError("Seek failed at t=" + std::to_string(seconds));
        return false;
    }
    avcodec_flush_buffers(codecCtx_);
    return true;
}

} // namespace nova::media

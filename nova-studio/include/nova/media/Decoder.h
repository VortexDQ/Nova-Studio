#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward-declare FFmpeg C types so this header doesn't leak <libavformat/...>
// into every translation unit that just wants to decode a frame.
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace nova::media {

// A single decoded video frame, already converted to packed RGBA8 so the
// renderer can upload it straight into a GPU texture without a second
// conversion pass. Real production code would keep frames in their native
// pixel format (e.g. NV12/YUV420P) and do the YUV->RGB conversion on the GPU
// in a shader for performance and HDR correctness; RGBA8-on-CPU is the
// correct simplification for a first vertical slice.
struct VideoFrame {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
    int64_t pts = 0;       // presentation timestamp, in stream time_base units
    double timeSeconds = 0.0;
};

struct MediaInfo {
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    bool hasVideo = false;
    bool hasAudio = false;
};

// Wraps an FFmpeg demux/decode pipeline for a single video file and exposes
// simple "give me the next frame" / "seek to time" operations. Not
// thread-safe; callers (e.g. the playback engine) are expected to own a
// Decoder per decode thread.
class Decoder {
public:
    Decoder();
    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;

    // Opens a media file and finds the best video stream. Returns false and
    // populates lastError() on failure.
    bool open(const std::string& path);
    void close();
    bool isOpen() const;

    const MediaInfo& info() const { return info_; }
    const std::string& lastError() const { return lastError_; }

    // Decodes and returns the next video frame in the stream, or std::nullopt
    // at end-of-stream / on error.
    std::optional<VideoFrame> nextFrame();

    // Seeks the underlying stream to the nearest keyframe at or before
    // `seconds`, then decodes forward. Real proxy/frame-accurate seeking
    // additionally decodes past the keyframe to the exact requested frame;
    // that refinement is left as a follow-up (see docs/ROADMAP.md).
    bool seek(double seconds);

private:
    void setError(const std::string& message);
    bool decodeNextPacket(AVFrame* frame);

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    MediaInfo info_;
    std::string lastError_;
};

} // namespace nova::media

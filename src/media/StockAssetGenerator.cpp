#include "nova/media/StockAssetGenerator.h"
#include "nova/media/ImageIO.h"
#include "nova/core/Logger.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <filesystem>
#include <functional>
#include <random>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace nova::media {

namespace {
constexpr const char* kModule = "media.StockAssetGenerator";

QImage makeLabeledPlate(int width, int height, QColor bg, const QString& label) {
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(bg);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(bg.lightness() > 140 ? QColor(20, 20, 24) : QColor(245, 245, 248));
    QFont font = painter.font();
    font.setPointSize(42);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, label);
    painter.setPen(QColor(255, 255, 255, 70));
    painter.drawRect(image.rect().adjusted(2, 2, -3, -3));
    return image;
}

QImage makeParticlesFrame(int width, int height, QColor bg, const QString& label, int frameIndex,
                          int totalFrames) {
    QImage image = makeLabeledPlate(width, height, bg, label);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 210));

    std::mt19937 rng(static_cast<unsigned>(frameIndex * 7919 + 104729));
    std::uniform_real_distribution<double> xDist(0.0, static_cast<double>(width));
    std::uniform_real_distribution<double> yDist(0.0, static_cast<double>(height));
    std::uniform_real_distribution<double> rDist(2.0, 6.0);

    const int particleCount = 80;
    for (int i = 0; i < particleCount; ++i) {
        const double phase = (frameIndex + i * 13) / static_cast<double>(totalFrames);
        const double x = std::fmod(xDist(rng) + phase * width * 0.35, static_cast<double>(width));
        const double y = std::fmod(yDist(rng) + std::sin(phase * 6.28 + i) * 40.0,
                                    static_cast<double>(height));
        const double radius = rDist(rng);
        painter.drawEllipse(QPointF(x, y), radius, radius);
    }
    return image;
}

QImage makeGradientFrame(int width, int height, const QString& label, int frameIndex,
                         int totalFrames, QColor base) {
    QImage image(width, height, QImage::Format_RGB32);
    const double shift = frameIndex / static_cast<double>(std::max(1, totalFrames));

    for (int y = 0; y < height; ++y) {
        const double rowMix = y / static_cast<double>(height);
        for (int x = 0; x < width; ++x) {
            const double colMix = x / static_cast<double>(width);
            const int r = static_cast<int>(std::clamp(
                base.red() * (0.4 + colMix * 0.6) + 80.0 * std::sin((colMix + shift) * 6.28), 0.0,
                255.0));
            const int g = static_cast<int>(std::clamp(
                base.green() * (0.3 + rowMix * 0.7) + 40.0 * std::cos((rowMix + shift) * 6.28),
                0.0, 255.0));
            const int b = static_cast<int>(std::clamp(
                base.blue() * (0.5 + (1.0 - colMix) * 0.5) + 30.0 * std::sin((rowMix - shift) * 4.0),
                0.0, 255.0));
            image.setPixelColor(x, y, QColor(r, g, b));
        }
    }

    QPainter painter(&image);
    painter.setPen(QColor(255, 255, 255, 220));
    QFont font = painter.font();
    font.setPointSize(42);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, label);
    return image;
}

bool writeVideoFromFrames(const std::string& path,
                          const std::function<QImage(int frameIndex, int totalFrames)>& frameFn,
                          double durationSec, double fps, int width, int height) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const int totalFrames = std::max(1, static_cast<int>(std::llround(durationSec * fps)));

    AVFormatContext* formatCtx = nullptr;
    if (avformat_alloc_output_context2(&formatCtx, nullptr, "mp4", path.c_str()) < 0 || !formatCtx) {
        NOVA_LOG_ERROR(kModule, "Failed to allocate output context");
        return false;
    }

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        NOVA_LOG_ERROR(kModule, "No H.264/MPEG4 encoder available");
        avformat_free_context(formatCtx);
        return false;
    }

    AVStream* stream = avformat_new_stream(formatCtx, codec);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    codecCtx->width = width;
    codecCtx->height = height;
    codecCtx->time_base = AVRational{1, static_cast<int>(fps)};
    codecCtx->framerate = AVRational{static_cast<int>(fps), 1};
    codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx->gop_size = 12;
    codecCtx->max_b_frames = 0;
    codecCtx->bit_rate = 4'000'000;
    if (formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        NOVA_LOG_ERROR(kModule, "Failed to open video encoder");
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        return false;
    }

    avcodec_parameters_from_context(stream->codecpar, codecCtx);
    stream->time_base = codecCtx->time_base;

    if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatCtx->pb, path.c_str(), AVIO_FLAG_WRITE) < 0) {
            NOVA_LOG_ERROR(kModule, "Failed to open output file");
            avcodec_free_context(&codecCtx);
            avformat_free_context(formatCtx);
            return false;
        }
    }

    if (avformat_write_header(formatCtx, nullptr) < 0) {
        NOVA_LOG_ERROR(kModule, "Failed to write header");
        if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&formatCtx->pb);
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        return false;
    }

    AVFrame* rgbFrame = av_frame_alloc();
    AVFrame* yuvFrame = av_frame_alloc();
    yuvFrame->format = codecCtx->pix_fmt;
    yuvFrame->width = width;
    yuvFrame->height = height;
    av_frame_get_buffer(yuvFrame, 32);

    SwsContext* sws = sws_getContext(width, height, AV_PIX_FMT_BGRA, width, height,
                                     AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);

    AVPacket* packet = av_packet_alloc();
    bool ok = true;

    for (int frameIndex = 0; frameIndex < totalFrames && ok; ++frameIndex) {
        const QImage qImage = frameFn(frameIndex, totalFrames).convertToFormat(QImage::Format_ARGB32);
        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, qImage.constBits(), AV_PIX_FMT_BGRA,
                             width, height, 1);
        sws_scale(sws, rgbFrame->data, rgbFrame->linesize, 0, height, yuvFrame->data,
                  yuvFrame->linesize);
        yuvFrame->pts = frameIndex;

        if (avcodec_send_frame(codecCtx, yuvFrame) < 0) {
            ok = false;
            break;
        }

        while (ok) {
            const int ret = avcodec_receive_packet(codecCtx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                ok = false;
                break;
            }
            av_packet_rescale_ts(packet, codecCtx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            if (av_interleaved_write_frame(formatCtx, packet) < 0) ok = false;
            av_packet_unref(packet);
        }
    }

    avcodec_send_frame(codecCtx, nullptr);
    while (ok && avcodec_receive_packet(codecCtx, packet) == 0) {
        av_packet_rescale_ts(packet, codecCtx->time_base, stream->time_base);
        packet->stream_index = stream->index;
        if (av_interleaved_write_frame(formatCtx, packet) < 0) ok = false;
        av_packet_unref(packet);
    }

    av_write_trailer(formatCtx);

    av_packet_free(&packet);
    av_frame_free(&rgbFrame);
    av_frame_free(&yuvFrame);
    sws_freeContext(sws);
    if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&formatCtx->pb);
    avcodec_free_context(&codecCtx);
    avformat_free_context(formatCtx);
    return ok;
}
} // namespace

bool StockAssetGenerator::generateColorPlate(const std::string& path, int r, int g, int b,
                                             const std::string& label, int width, int height) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const QImage image = makeLabeledPlate(width, height, QColor(r, g, b),
                                          QString::fromStdString(label));
    if (ImageIO::saveBmp(path, image)) return true;

    const bool saved = image.save(QString::fromStdString(path), "BMP");
    if (!saved) {
        NOVA_LOG_ERROR(kModule, "Failed to save stock plate: " + path);
    }
    return saved;
}

bool StockAssetGenerator::generateParticlesVideo(const std::string& path, int r, int g, int b,
                                                 const std::string& label, double durationSec,
                                                 double fps, int width, int height) {
    const QColor bg(r, g, b);
    const QString qLabel = QString::fromStdString(label);
    return writeVideoFromFrames(
        path,
        [&](int frameIndex, int totalFrames) {
            return makeParticlesFrame(width, height, bg, qLabel, frameIndex, totalFrames);
        },
        durationSec, fps, width, height);
}

bool StockAssetGenerator::generateGradientVideo(const std::string& path, int r, int g, int b,
                                                const std::string& label, double durationSec,
                                                double fps, int width, int height) {
    const QColor base(r, g, b);
    const QString qLabel = QString::fromStdString(label);
    return writeVideoFromFrames(
        path,
        [&](int frameIndex, int totalFrames) {
            return makeGradientFrame(width, height, qLabel, frameIndex, totalFrames, base);
        },
        durationSec, fps, width, height);
}

} // namespace nova::media

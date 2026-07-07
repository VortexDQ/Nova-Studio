#include "nova/media/AudioExtractor.h"
#include "nova/core/Logger.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace nova::media {

namespace {
constexpr const char* kModule = "media.AudioExtractor";
constexpr int kOutputSampleRate = 48000;
constexpr int kOutputChannels = 2;

std::string avError(int code) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    av_strerror(code, buffer.data(), buffer.size());
    return buffer.data();
}

void writeLe16(std::ofstream& out, uint16_t value) {
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
}

void writeLe32(std::ofstream& out, uint32_t value) {
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>((value >> 16) & 0xff));
    out.put(static_cast<char>((value >> 24) & 0xff));
}

void writeWavHeader(std::ofstream& out, uint32_t dataBytes) {
    constexpr uint16_t bitsPerSample = 16;
    constexpr uint16_t blockAlign = kOutputChannels * bitsPerSample / 8;
    constexpr uint32_t byteRate = kOutputSampleRate * blockAlign;

    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    writeLe32(out, 36 + dataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeLe32(out, 16);
    writeLe16(out, 1); // PCM
    writeLe16(out, kOutputChannels);
    writeLe32(out, kOutputSampleRate);
    writeLe32(out, byteRate);
    writeLe16(out, blockAlign);
    writeLe16(out, bitsPerSample);
    out.write("data", 4);
    writeLe32(out, dataBytes);
}
} // namespace

void AudioExtractor::setError(const std::string& message) {
    lastError_ = message;
    NOVA_LOG_ERROR(kModule, message);
}

bool AudioExtractor::extractWav(const std::string& inputPath, const std::string& outputPath) {
    lastError_.clear();

    AVFormatContext* format = nullptr;
    int ret = avformat_open_input(&format, inputPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        setError("Failed to open input: " + avError(ret));
        return false;
    }

    ret = avformat_find_stream_info(format, nullptr);
    if (ret < 0) {
        setError("Failed to read stream info: " + avError(ret));
        avformat_close_input(&format);
        return false;
    }

    const int audioStream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStream < 0) {
        setError("No audio stream found.");
        avformat_close_input(&format);
        return false;
    }

    AVStream* stream = format->streams[audioStream];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        setError("No decoder available for audio stream.");
        avformat_close_input(&format);
        return false;
    }

    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        setError("Failed to allocate audio decoder context.");
        avformat_close_input(&format);
        return false;
    }

    ret = avcodec_parameters_to_context(codecContext, stream->codecpar);
    if (ret < 0) {
        setError("Failed to configure audio decoder: " + avError(ret));
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    ret = avcodec_open2(codecContext, codec, nullptr);
    if (ret < 0) {
        setError("Failed to open audio decoder: " + avError(ret));
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    AVChannelLayout outputLayout{};
    av_channel_layout_default(&outputLayout, kOutputChannels);

    AVChannelLayout inputLayout{};
    if (codecContext->ch_layout.nb_channels > 0) {
        av_channel_layout_copy(&inputLayout, &codecContext->ch_layout);
    } else {
        av_channel_layout_default(&inputLayout, codecContext->ch_layout.nb_channels > 0
                                                ? codecContext->ch_layout.nb_channels
                                                : kOutputChannels);
    }

    SwrContext* swr = nullptr;
    ret = swr_alloc_set_opts2(&swr,
                              &outputLayout,
                              AV_SAMPLE_FMT_S16,
                              kOutputSampleRate,
                              &inputLayout,
                              codecContext->sample_fmt,
                              codecContext->sample_rate,
                              0,
                              nullptr);
    av_channel_layout_uninit(&inputLayout);
    av_channel_layout_uninit(&outputLayout);

    if (ret < 0 || !swr) {
        setError("Failed to create audio resampler: " + avError(ret));
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    ret = swr_init(swr);
    if (ret < 0) {
        setError("Failed to initialize audio resampler: " + avError(ret));
        swr_free(&swr);
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    const auto parentPath = std::filesystem::path(outputPath).parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }
    std::ofstream wav(outputPath, std::ios::binary | std::ios::trunc);
    if (!wav) {
        setError("Failed to create output file: " + outputPath);
        swr_free(&swr);
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }
    writeWavHeader(wav, 0);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!packet || !frame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        setError("Failed to allocate audio decode buffers.");
        swr_free(&swr);
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    uint32_t dataBytes = 0;
    auto receiveFrames = [&]() -> bool {
        while (true) {
            ret = avcodec_receive_frame(codecContext, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return true;
            }
            if (ret < 0) {
                setError("Audio decode failed: " + avError(ret));
                return false;
            }

            const auto outSamples64 = av_rescale_rnd(
                swr_get_delay(swr, codecContext->sample_rate) + frame->nb_samples,
                kOutputSampleRate,
                codecContext->sample_rate,
                AV_ROUND_UP);
            const int outSamples = static_cast<int>(outSamples64);

            std::vector<uint8_t> pcm(static_cast<size_t>(outSamples) * kOutputChannels * sizeof(int16_t));
            uint8_t* outData[] = { pcm.data() };
            const int converted = swr_convert(swr, outData, outSamples,
                                              (const uint8_t**)frame->extended_data,
                                              frame->nb_samples);
            if (converted < 0) {
                setError("Audio resample failed: " + avError(converted));
                return false;
            }

            const size_t bytes = static_cast<size_t>(converted) * kOutputChannels * sizeof(int16_t);
            wav.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(bytes));
            dataBytes += static_cast<uint32_t>(bytes);
            av_frame_unref(frame);
        }
    };

    while (av_read_frame(format, packet) >= 0) {
        if (packet->stream_index == audioStream) {
            ret = avcodec_send_packet(codecContext, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                setError("Failed to send audio packet: " + avError(ret));
                av_packet_free(&packet);
                av_frame_free(&frame);
                swr_free(&swr);
                avcodec_free_context(&codecContext);
                avformat_close_input(&format);
                return false;
            }
            if (!receiveFrames()) {
                av_packet_unref(packet);
                av_packet_free(&packet);
                av_frame_free(&frame);
                swr_free(&swr);
                avcodec_free_context(&codecContext);
                avformat_close_input(&format);
                return false;
            }
        }
        av_packet_unref(packet);
    }

    avcodec_send_packet(codecContext, nullptr);
    if (!receiveFrames()) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        swr_free(&swr);
        avcodec_free_context(&codecContext);
        avformat_close_input(&format);
        return false;
    }

    writeWavHeader(wav, dataBytes);
    wav.close();
    av_packet_free(&packet);
    av_frame_free(&frame);
    swr_free(&swr);
    avcodec_free_context(&codecContext);
    avformat_close_input(&format);

    NOVA_LOG_INFO(kModule, "Extracted WAV audio to " + outputPath);
    return true;
}

} // namespace nova::media

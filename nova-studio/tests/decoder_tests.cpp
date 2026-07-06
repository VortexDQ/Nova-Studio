// Generates a tiny synthetic MP4 with the ffmpeg CLI (lavfi testsrc) at test
// time, then exercises Decoder against it. This keeps the test suite
// dependency-free (no binary test fixtures checked into the repo) while
// still testing real decode/seek behavior end-to-end.
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "nova/media/Decoder.h"

namespace fs = std::filesystem;

namespace {
int g_failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++g_failures;
    } else {
        std::cout << "ok - " << description << '\n';
    }
}
} // namespace

int main() {
    fs::path testFile = fs::temp_directory_path() / "nova_decoder_test_clip.mp4";

    std::string cmd = "ffmpeg -y -loglevel error -f lavfi -i "
                       "testsrc=duration=2:size=320x240:rate=25 "
                       "-pix_fmt yuv420p " + testFile.string();
    int genResult = std::system(cmd.c_str());
    if (genResult != 0 || !fs::exists(testFile)) {
        std::cerr << "SKIP: could not generate synthetic test clip with ffmpeg CLI "
                      "(command failed or binary unavailable); skipping decoder tests.\n";
        return 0;
    }

    nova::media::Decoder decoder;
    expect(decoder.open(testFile.string()), "decoder opens synthetic test clip");
    expect(decoder.isOpen(), "decoder reports open after successful open()");

    const auto& info = decoder.info();
    expect(info.width == 320, "decoded width matches source (320)");
    expect(info.height == 240, "decoded height matches source (240)");
    expect(info.hasVideo, "media info reports a video stream");

    auto frame = decoder.nextFrame();
    expect(frame.has_value(), "nextFrame() returns a frame for the first frame");
    if (frame) {
        expect(frame->width == 320 && frame->height == 240, "frame dimensions match source");
        expect(frame->rgba.size() == static_cast<size_t>(320 * 240 * 4),
               "frame buffer size matches width*height*4 (RGBA8)");
    }

    int decodedCount = 1;
    while (decoder.nextFrame().has_value()) ++decodedCount;
    expect(decodedCount > 1, "decoder can walk through multiple frames to end-of-stream");

    expect(decoder.seek(0.0), "seek back to start succeeds after reaching EOF");
    auto afterSeek = decoder.nextFrame();
    expect(afterSeek.has_value(), "a frame is decodable immediately after seeking to 0");

    decoder.close();
    expect(!decoder.isOpen(), "decoder reports closed after close()");

    std::error_code ec;
    fs::remove(testFile, ec);

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All decoder tests passed.\n";
    return 0;
}

#pragma once

#include <string>

namespace nova::media {

class GifExporter {
public:
    bool exportGif(const std::string& inputPath,
                   const std::string& outputPath,
                   double startSeconds,
                   double endSeconds,
                   int fps = 10,
                   int maxWidth = 640,
                   std::string* error = nullptr);

    const std::string& lastError() const { return lastError_; }

private:
    std::string lastError_;
};

} // namespace nova::media

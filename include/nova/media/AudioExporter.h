#pragma once

#include <string>

namespace nova::media {

class AudioExporter {
public:
    bool exportMp3(const std::string& inputPath,
                   const std::string& outputPath,
                   double startSeconds = 0.0,
                   double endSeconds = 0.0,
                   std::string* error = nullptr);

    const std::string& lastError() const { return lastError_; }

private:
    std::string lastError_;
};

} // namespace nova::media

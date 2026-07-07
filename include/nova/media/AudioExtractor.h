#pragma once

#include <string>

namespace nova::media {

class AudioExtractor {
public:
    bool extractWav(const std::string& inputPath, const std::string& outputPath);

    const std::string& lastError() const { return lastError_; }

private:
    void setError(const std::string& message);

    std::string lastError_;
};

} // namespace nova::media

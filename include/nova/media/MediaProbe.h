#pragma once

#include <string>

namespace nova::media {

struct FileMetadata {
    std::string path;
    std::string fileName;
    std::string formatName;   // container or image type
    std::string videoCodec;
    std::string audioCodec;
    int64_t fileSizeBytes = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    bool hasVideo = false;
    bool hasAudio = false;
    bool isImage = false;
};

class MediaProbe {
public:
    static FileMetadata probe(const std::string& path);
};

} // namespace nova::media

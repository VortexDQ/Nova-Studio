#pragma once

#include <string>

namespace nova::media {

enum class ExportFormat { Mp4, Mov, Mkv, Webm, Avi };

class VideoExporter {
public:
    bool exportClip(const std::string& inputPath,
                    const std::string& outputPath,
                    ExportFormat format,
                    double startSeconds,
                    double endSeconds,
                    std::string* error = nullptr);

    static std::string extensionFor(ExportFormat format);
};

} // namespace nova::media

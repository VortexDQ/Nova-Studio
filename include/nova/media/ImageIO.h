#pragma once

#include <QImage>
#include <string>

namespace nova::media {

// Minimal BMP read/write that does not depend on Qt image format plugins.
class ImageIO {
public:
    static bool saveBmp(const std::string& path, const QImage& image);
    static bool loadBmp(const std::string& path, QImage& outImage);
};

} // namespace nova::media

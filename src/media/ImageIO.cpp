#include "nova/media/ImageIO.h"

#include <QFile>
#include <cstring>
#include <vector>

namespace nova::media {

namespace {
#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t type = 0x4D42;
    uint32_t size = 0;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t offset = 54;
};

struct BmpInfoHeader {
    uint32_t size = 40;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 1;
    uint16_t bitCount = 24;
    uint32_t compression = 0;
    uint32_t imageSize = 0;
    int32_t xPelsPerMeter = 2835;
    int32_t yPelsPerMeter = 2835;
    uint32_t clrUsed = 0;
    uint32_t clrImportant = 0;
};
#pragma pack(pop)
} // namespace

bool ImageIO::saveBmp(const std::string& path, const QImage& image) {
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) return false;

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const int width = rgb.width();
    const int height = rgb.height();
    const int rowStride = ((width * 3 + 3) / 4) * 4;
    const uint32_t imageSize = static_cast<uint32_t>(rowStride * height);

    BmpFileHeader fileHeader;
    BmpInfoHeader infoHeader;
    fileHeader.size = 54 + imageSize;
    infoHeader.width = width;
    infoHeader.height = height;
    infoHeader.imageSize = imageSize;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly)) return false;

    if (file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader)) != sizeof(fileHeader)) {
        return false;
    }
    if (file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader)) != sizeof(infoHeader)) {
        return false;
    }

    std::vector<char> row(static_cast<size_t>(rowStride), 0);
    for (int y = height - 1; y >= 0; --y) {
        const uchar* src = rgb.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            row[static_cast<size_t>(x * 3) + 0] = static_cast<char>(src[x * 3 + 2]);
            row[static_cast<size_t>(x * 3) + 1] = static_cast<char>(src[x * 3 + 1]);
            row[static_cast<size_t>(x * 3) + 2] = static_cast<char>(src[x * 3 + 0]);
        }
        if (file.write(row.data(), rowStride) != rowStride) return false;
    }
    return true;
}

bool ImageIO::loadBmp(const std::string& path, QImage& outImage) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return false;

    BmpFileHeader fileHeader;
    BmpInfoHeader infoHeader;
    if (file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader)) != sizeof(fileHeader)) return false;
    if (file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader)) != sizeof(infoHeader)) return false;
    if (fileHeader.type != 0x4D42 || infoHeader.bitCount != 24 || infoHeader.compression != 0) return false;
    if (infoHeader.width <= 0 || infoHeader.height <= 0) return false;

    const int width = infoHeader.width;
    const int height = infoHeader.height;
    const int rowStride = ((width * 3 + 3) / 4) * 4;
    QImage image(width, height, QImage::Format_RGB888);

    std::vector<char> row(static_cast<size_t>(rowStride));
    for (int y = height - 1; y >= 0; --y) {
        if (file.read(row.data(), rowStride) != rowStride) return false;
        uchar* dst = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            dst[x * 3 + 0] = static_cast<uchar>(row[static_cast<size_t>(x * 3) + 2]);
            dst[x * 3 + 1] = static_cast<uchar>(row[static_cast<size_t>(x * 3) + 1]);
            dst[x * 3 + 2] = static_cast<uchar>(row[static_cast<size_t>(x * 3) + 0]);
        }
    }

    outImage = image;
    return true;
}

} // namespace nova::media

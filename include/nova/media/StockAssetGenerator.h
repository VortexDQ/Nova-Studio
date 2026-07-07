#pragma once

#include <string>

namespace nova::media {

class StockAssetGenerator {
public:
    static bool generateColorPlate(const std::string& path, int r, int g, int b,
                                   const std::string& label, int width = 1920, int height = 1080);

    static bool generateParticlesVideo(const std::string& path, int r, int g, int b,
                                       const std::string& label, double durationSec = 5.0,
                                       double fps = 30.0, int width = 1920, int height = 1080);

    static bool generateGradientVideo(const std::string& path, int r, int g, int b,
                                      const std::string& label, double durationSec = 5.0,
                                      double fps = 30.0, int width = 1920, int height = 1080);
};

} // namespace nova::media

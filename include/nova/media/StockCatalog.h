#pragma once

namespace nova::media {

struct StockAssetDef {
    const char* id;
    const char* label;
    int r;
    int g;
    int b;
    bool isVideo;
};

inline constexpr StockAssetDef kStockAssets[] = {
    {"black-5s", "Black background (5s)", 0, 0, 0, false},
    {"white-5s", "White background (5s)", 255, 255, 255, false},
    {"blue-5s", "Blue background (5s)", 30, 90, 200, false},
    {"gray-5s", "Gray background (5s)", 120, 120, 125, false},
    {"red-5s", "Red background (5s)", 180, 40, 40, false},
    {"particles-5s", "Floating particles (5s)", 20, 30, 80, true},
    {"gradient-5s", "Sunset gradient (5s)", 200, 90, 40, true},
};

inline const StockAssetDef* findStockAsset(const char* id) {
    for (const auto& asset : kStockAssets) {
        if (asset.id == id) return &asset;
    }
    return nullptr;
}

} // namespace nova::media

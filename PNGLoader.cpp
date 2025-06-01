#include "GUIState.h"
#include <vector>
#include <string>

struct Image2D {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;

    bool IsOpaque(int x, int y) const {
        return pixels[(y * width + x) * 4 + 3] > 128;
    }
};

Image2D LoadPNG(const char* path) {
    // 仮実装：本来は自作 PNG inflate 実装を入れる
    Image2D img;
    img.width = 64;
    img.height = 64;
    img.pixels.resize(img.width * img.height * 4, 255); // 全白透過なし
    return img;
}

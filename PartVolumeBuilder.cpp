#include "PartVolumeBuilder.h"
#include "VolumeBuilder.h"

std::vector<PartVolume> BuildPartsFromImage(const Image2D& image, const std::vector<PartRegion>& regions, int depth) {
    std::vector<PartVolume> result;

    for (const auto& region : regions) {
        // êÿÇËî≤Ç´
        Image2D cropped;
        cropped.width = region.width;
        cropped.height = region.height;
        cropped.pixels.resize(region.width * region.height * 4);

        for (int y = 0; y < region.height; ++y) {
            for (int x = 0; x < region.width; ++x) {
                int srcX = region.x + x;
                int srcY = region.y + y;
                if (srcX < 0 || srcX >= image.width || srcY < 0 || srcY >= image.height) continue;

                int srcIdx = (srcY * image.width + srcX) * 4;
                int dstIdx = (y * region.width + x) * 4;
                std::copy_n(&image.pixels[srcIdx], 4, &cropped.pixels[dstIdx]);
            }
        }

        // ï™óﬁ
        PartType type = ClassifyPart(region.x, region.y, region.width, region.height, image.width, image.height);

        // Volumeê∂ê¨
        Volume vol = BuildVolumeFromSilhouette(cropped, depth);

        result.push_back({ type, vol });
    }

    return result;
}

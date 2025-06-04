#include "PartProcessor.h"
#include "VolumeUtils.h"

PartProcessor::PartProcessor() {}

void PartProcessor::setSelectionRegions(const std::vector<SelectionRegion>& regions) {
    this->regions = regions;
}

VolumeData* PartProcessor::process(VolumeData* inputVolume) {
    if (!inputVolume) return nullptr;

    const int sizeX = inputVolume->getSizeX();
    const int sizeY = inputVolume->getSizeY();
    const int sizeZ = inputVolume->getSizeZ();

    VolumeData* result = new VolumeData(sizeX, sizeY, sizeZ);

    // 全領域を一度クリア
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                result->set(x, y, z, false);

    // 各矩形領域に対応する範囲を抽出（XY平面上）
    for (const auto& region : regions) {
        int xStart = region.x * sizeX / 512;
        int yStart = region.y * sizeY / 512;
        int xEnd = (region.x + region.width) * sizeX / 512;
        int yEnd = (region.y + region.height) * sizeY / 512;

        for (int z = 0; z < sizeZ; ++z) {
            for (int y = yStart; y < yEnd && y < sizeY; ++y) {
                for (int x = xStart; x < xEnd && x < sizeX; ++x) {
                    if (inputVolume->get(x, y, z)) {
                        result->set(x, y, z, true);
                    }
                }
            }
        }
    }

    return result;
}

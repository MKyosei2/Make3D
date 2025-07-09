#pragma once
#include <windows.h>
#include <vector>

struct VolumeData {
    int width = 0;
    int height = 0;
    int depth = 0;
    std::vector<bool> data;

    VolumeData(int w = 0, int h = 0, int d = 0)
        : width(w), height(h), depth(d), data(w* h* d, false) {
    }

    bool get(int x, int y, int z) const {
        if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth) return false;
        return data[x + y * width + z * width * height];
    }

    void set(int x, int y, int z, bool value) {
        if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth) return;
        data[x + y * width + z * width * height] = value;
    }
};

class VolumeBuilder
{
public:
    VolumeBuilder();
    ~VolumeBuilder();

    bool buildVolume(HBITMAP images[6], int silhouetteThreshold);
    const VolumeData& getVolume() const;

private:
    VolumeData volume = VolumeData(128, 128, 128); // 仮ボリュームサイズ
};

#include "GUIState.h"

struct Volume {
    int w = 0, h = 0, d = 0;
    std::vector<uint8_t> voxels;

    void Resize(int x, int y, int z) {
        w = x; h = y; d = z;
        voxels.resize(w * h * d, 0);
    }
    uint8_t& At(int x, int y, int z) {
        return voxels[(z * h + y) * w + x];
    }
};

Volume BuildVolumeFromSilhouette(const Image2D& img, ViewType view) {
    Volume vol;
    const int depth = 32;
    vol.Resize(img.width, img.height, depth);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            if (!img.IsOpaque(x, y)) continue;
            for (int z = 0; z < depth; ++z) {
                vol.At(x, y, z) = 255;
            }
        }
    }
    return vol;
}

Mesh3D BuildMeshFromVolume(const Volume& vol) {
    Mesh3D mesh;
    // 仮実装：1枚板ポリゴンで代用
    for (int y = 0; y < vol.h; ++y) {
        for (int x = 0; x < vol.w; ++x) {
            if (vol.At(x, y, vol.d / 2)) {
                float fx = (float)x, fy = (float)y, fz = (float)(vol.d / 2);
                int idx = (int)mesh.vertices.size();
                mesh.vertices.push_back({ fx, fy, fz });
                mesh.vertices.push_back({ fx + 1, fy, fz });
                mesh.vertices.push_back({ fx + 1, fy + 1, fz });
                mesh.vertices.push_back({ fx, fy + 1, fz });
                mesh.indices.push_back(idx); mesh.indices.push_back(idx + 1); mesh.indices.push_back(idx + 2);
                mesh.indices.push_back(idx); mesh.indices.push_back(idx + 2); mesh.indices.push_back(idx + 3);
            }
        }
    }
    return mesh;
}

std::map<PartType, Volume> BuildVolumesFromImages(const std::map<PartType, std::map<ViewType, std::vector<std::string>>>& inputMap) {
    std::map<PartType, Volume> result;
    for (const auto& [part, views] : inputMap) {
        Volume merged;
        for (const auto& [view, files] : views) {
            for (const auto& f : files) {
                Image2D img = LoadPNG(f.c_str());
                Volume vol = BuildVolumeFromSilhouette(img, view);
                merged = vol; // 簡略：最後の1枚のみ使用
            }
        }
        result[part] = merged;
    }
    return result;
}

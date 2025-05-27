#include "VolumeBuilder.h"
#include <algorithm>

static void ProjectSilhouetteXY(Volume& vol, const Image2D& img, int zStart, int zEnd, bool reverseZ) {
    for (int z = zStart; z != zEnd; z += (reverseZ ? -1 : 1)) {
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                int idx2D = (y * img.width + x) * 4;
                unsigned char alpha = img.pixels[idx2D + 3];
                int idx3D = z * vol.width * vol.height + y * vol.width + x;
                if (alpha > 0) vol.voxels[idx3D] = 255;
            }
        }
    }
}

static void ProjectSilhouetteYZ(Volume& vol, const Image2D& img, int xStart, int xEnd, bool reverseX) {
    for (int x = xStart; x != xEnd; x += (reverseX ? -1 : 1)) {
        for (int y = 0; y < img.height; ++y) {
            for (int z = 0; z < img.width; ++z) {
                int idx2D = (y * img.width + z) * 4;
                unsigned char alpha = img.pixels[idx2D + 3];
                int idx3D = z * vol.width * vol.height + y * vol.width + x;
                if (alpha > 0) vol.voxels[idx3D] = 255;
            }
        }
    }
}

static void ProjectSilhouetteXZ(Volume& vol, const Image2D& img, int yStart, int yEnd, bool reverseY) {
    for (int y = yStart; y != yEnd; y += (reverseY ? -1 : 1)) {
        for (int z = 0; z < img.height; ++z) {
            for (int x = 0; x < img.width; ++x) {
                int idx2D = (z * img.width + x) * 4;
                unsigned char alpha = img.pixels[idx2D + 3];
                int idx3D = z * vol.width * vol.height + y * vol.width + x;
                if (alpha > 0) vol.voxels[idx3D] = 255;
            }
        }
    }
}

Volume BuildVolumeFromMultipleSilhouettes(
    const Image2D* front,
    const Image2D* back,
    const Image2D* right,
    const Image2D* left,
    const Image2D* top,
    const Image2D* bottom,
    int depth)
{
    // 基準サイズを決定（frontが必須）
    if (!front) throw std::runtime_error("Front image is required");

    int W = front->width;
    int H = front->height;
    int D = depth;

    Volume vol{ W, H, D };
    vol.voxels.resize(W * H * D, 0);

    int midZ = D / 2;
    int midX = W / 2;
    int midY = H / 2;

    if (front)  ProjectSilhouetteXY(vol, *front, midZ, D, false);
    if (back)   ProjectSilhouetteXY(vol, *back, midZ - 1, -1, true);
    if (right)  ProjectSilhouetteYZ(vol, *right, midX - 1, -1, true);
    if (left)   ProjectSilhouetteYZ(vol, *left, midX, W, false);
    if (top)    ProjectSilhouetteXZ(vol, *top, midY - 1, -1, true);
    if (bottom) ProjectSilhouetteXZ(vol, *bottom, midY, H, false);

    return vol;
}

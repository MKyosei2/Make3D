#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <windows.h>
#include <commdlg.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <atomic>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

struct ImageRGBA {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

struct DepthImage {
    int width = 0;
    int height = 0;
    std::vector<float> values;
};

struct MeshData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<int> indices;
};

struct VideoReconstructionSettings {
    int targetFrameCount = 24;
    int voxelResolution = 84;
    float silhouetteThreshold = 34.0f;
    int morphologyPasses = 2;
    int meshSmoothIterations = 3;
};

static void ShowError(const std::string& message) {
    MessageBoxA(nullptr, message.c_str(), "Make3D Portable", MB_ICONERROR | MB_OK);
}
static void ShowInfo(const std::string& message) {
    MessageBoxA(nullptr, message.c_str(), "Make3D Portable", MB_ICONINFORMATION | MB_OK);
}

static std::vector<std::string> OpenMultiplePngFiles(const char* title) {
    char buffer[65536] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = sizeof(buffer);
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;

    if (!GetOpenFileNameA(&ofn)) return {};

    std::vector<std::string> result;
    const char* ptr = buffer;
    std::string first = ptr;
    ptr += first.size() + 1;
    if (*ptr == '\0') {
        result.push_back(first);
        return result;
    }
    std::string directory = first;
    while (*ptr) {
        std::string fileName = ptr;
        result.push_back((fs::path(directory) / fileName).string());
        ptr += fileName.size() + 1;
    }
    return result;
}

static std::optional<std::string> OpenSingleMediaFile(const char* title) {
    char buffer[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Media Files (*.mp4;*.mov;*.avi;*.wmv)\0*.mp4;*.mov;*.avi;*.wmv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = sizeof(buffer);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;
    if (!GetOpenFileNameA(&ofn)) return std::nullopt;
    return std::string(buffer);
}

static std::optional<ImageRGBA> LoadRGBA(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) return std::nullopt;
    ImageRGBA img;
    img.width = w;
    img.height = h;
    img.pixels.assign(data, data + (size_t)w * h * 4);
    stbi_image_free(data);
    return img;
}

static std::optional<DepthImage> LoadDepth(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    if (!data) return std::nullopt;
    DepthImage depth;
    depth.width = w;
    depth.height = h;
    depth.values.resize((size_t)w * h);
    const int step = (comp <= 0) ? 1 : comp;
    for (int i = 0; i < w * h; ++i) depth.values[i] = data[i * step] / 255.0f;
    stbi_image_free(data);
    return depth;
}

static bool ValidatePairDimensions(const std::vector<ImageRGBA>& colors, const std::vector<DepthImage>& depths, std::string& reason) {
    if (colors.empty() || depths.empty()) { reason = "No input images were selected."; return false; }
    if (colors.size() != depths.size()) { reason = "The number of color images and depth images must match."; return false; }
    int baseW = colors.front().width, baseH = colors.front().height;
    for (size_t i = 0; i < colors.size(); ++i) {
        if (colors[i].width != baseW || colors[i].height != baseH) { reason = "All color images must share the same resolution."; return false; }
        if (depths[i].width != baseW || depths[i].height != baseH) { reason = "Every depth image must match the color image resolution."; return false; }
    }
    return true;
}

static std::vector<float> FuseDepthMedianWeighted(const std::vector<DepthImage>& depths) {
    const int w = depths.front().width;
    const int h = depths.front().height;
    std::vector<float> fused((size_t)w * h, 0.0f), samples;
    samples.reserve(depths.size());
    for (int idx = 0; idx < w * h; ++idx) {
        samples.clear();
        for (const auto& d : depths) samples.push_back(d.values[idx]);
        std::sort(samples.begin(), samples.end());
        float median = samples[samples.size() / 2];
        float weightedSum = 0.0f, weightedTotal = 0.0f;
        for (float s : samples) {
            float weight = 1.0f - std::min(1.0f, std::abs(s - median) * 4.0f);
            weight = std::max(0.05f, weight);
            weightedSum += s * weight;
            weightedTotal += weight;
        }
        fused[idx] = weightedTotal > 0.0f ? (weightedSum / weightedTotal) : median;
    }
    return fused;
}

static void FillDepthHoles(std::vector<float>& depth, const std::vector<unsigned char>& alphaMask, int w, int h) {
    std::vector<float> original = depth;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int idx = y * w + x;
            if (alphaMask[idx] == 0 || original[idx] > 0.001f) continue;
            float sum = 0.0f;
            int count = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    int ni = (y + oy) * w + (x + ox);
                    if (alphaMask[ni] == 0 || original[ni] <= 0.001f) continue;
                    sum += original[ni];
                    ++count;
                }
            }
            if (count > 0) depth[idx] = sum / count;
        }
    }
}

static void SmoothDepthEdgeAware(std::vector<float>& depth, const std::vector<unsigned char>& alphaMask, int w, int h) {
    std::vector<float> src = depth;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int idx = y * w + x;
            if (alphaMask[idx] == 0) continue;
            float center = src[idx], sum = center * 2.0f, weightSum = 2.0f;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (!ox && !oy) continue;
                    int ni = (y + oy) * w + (x + ox);
                    if (alphaMask[ni] == 0) continue;
                    float diff = std::abs(src[ni] - center);
                    float weight = diff < 0.06f ? 1.0f : 0.25f;
                    sum += src[ni] * weight;
                    weightSum += weight;
                }
            }
            depth[idx] = sum / weightSum;
        }
    }
}

static std::vector<unsigned char> BuildAlphaMaskUnion(const std::vector<ImageRGBA>& colors) {
    const int w = colors.front().width, h = colors.front().height;
    std::vector<unsigned char> mask((size_t)w * h, 0);
    for (const auto& img : colors) {
        for (int idx = 0; idx < w * h; ++idx) {
            if (img.pixels[(size_t)idx * 4 + 3] > 0) mask[idx] = 255;
        }
    }
    return mask;
}

static void AppendTri(std::vector<int>& indices, int a, int b, int c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

static MeshData BuildClosedDepthMesh(const std::vector<float>& fusedDepth, const std::vector<unsigned char>& alphaMask, int w, int h, float xyScale, float depthScale, float thickness) {
    MeshData mesh;
    std::vector<int> frontMap((size_t)w * h, -1), backMap((size_t)w * h, -1);
    auto addVertex = [&](float x, float y, float z, float u, float v) -> int {
        int index = (int)(mesh.positions.size() / 3);
        mesh.positions.insert(mesh.positions.end(), {x, y, z});
        mesh.normals.insert(mesh.normals.end(), {0, 0, 0});
        mesh.uvs.insert(mesh.uvs.end(), {u, v});
        return index;
    };
    const float denomW = (w > 1) ? float(w - 1) : 1.0f;
    const float denomH = (h > 1) ? float(h - 1) : 1.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (!alphaMask[idx]) continue;
            float nx = ((float)x / denomW - 0.5f) * xyScale;
            float ny = ((float)(h - 1 - y) / denomH - 0.5f) * xyScale;
            float nz = fusedDepth[idx] * depthScale;
            float u = (float)x / denomW, v = (float)y / denomH;
            frontMap[idx] = addVertex(nx, ny, nz, u, v);
            backMap[idx] = addVertex(nx, ny, nz - thickness, u, v);
        }
    }
    for (int y = 0; y < h - 1; ++y) {
        for (int x = 0; x < w - 1; ++x) {
            int i00 = y * w + x, i10 = y * w + (x + 1), i01 = (y + 1) * w + x, i11 = (y + 1) * w + (x + 1);
            if (frontMap[i00] >= 0 && frontMap[i10] >= 0 && frontMap[i11] >= 0) AppendTri(mesh.indices, frontMap[i00], frontMap[i10], frontMap[i11]);
            if (frontMap[i00] >= 0 && frontMap[i11] >= 0 && frontMap[i01] >= 0) AppendTri(mesh.indices, frontMap[i00], frontMap[i11], frontMap[i01]);
            if (backMap[i00] >= 0 && backMap[i11] >= 0 && backMap[i10] >= 0) AppendTri(mesh.indices, backMap[i00], backMap[i11], backMap[i10]);
            if (backMap[i00] >= 0 && backMap[i01] >= 0 && backMap[i11] >= 0) AppendTri(mesh.indices, backMap[i00], backMap[i01], backMap[i11]);
        }
    }
    auto appendSideQuad = [&](int a0, int a1, int b0, int b1) {
        if (a0 < 0 || a1 < 0 || b0 < 0 || b1 < 0) return;
        AppendTri(mesh.indices, a0, a1, b1);
        AppendTri(mesh.indices, a0, b1, b0);
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (!alphaMask[idx]) continue;
            if (x == 0 || !alphaMask[y * w + x - 1]) {
                int n = (y + 1 < h) ? ((y + 1) * w + x) : -1;
                appendSideQuad(frontMap[idx], frontMap[n], backMap[idx], backMap[n]);
            }
            if (x == w - 1 || !alphaMask[y * w + x + 1]) {
                int n = (y + 1 < h) ? ((y + 1) * w + x) : -1;
                if (n >= 0) {
                    AppendTri(mesh.indices, frontMap[idx], backMap[n], frontMap[n]);
                    AppendTri(mesh.indices, frontMap[idx], backMap[idx], backMap[n]);
                }
            }
            if (y == 0 || !alphaMask[(y - 1) * w + x]) {
                int n = (x + 1 < w) ? (y * w + x + 1) : -1;
                appendSideQuad(frontMap[idx], backMap[idx], frontMap[n], backMap[n]);
            }
            if (y == h - 1 || !alphaMask[(y + 1) * w + x]) {
                int n = (x + 1 < w) ? (y * w + x + 1) : -1;
                if (n >= 0) {
                    AppendTri(mesh.indices, frontMap[idx], frontMap[n], backMap[n]);
                    AppendTri(mesh.indices, frontMap[idx], backMap[n], backMap[idx]);
                }
            }
        }
    }
    return mesh;
}

static void RecomputeNormals(MeshData& mesh) {
    std::fill(mesh.normals.begin(), mesh.normals.end(), 0.0f);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        float ax = mesh.positions[ia * 3 + 0], ay = mesh.positions[ia * 3 + 1], az = mesh.positions[ia * 3 + 2];
        float bx = mesh.positions[ib * 3 + 0], by = mesh.positions[ib * 3 + 1], bz = mesh.positions[ib * 3 + 2];
        float cx = mesh.positions[ic * 3 + 0], cy = mesh.positions[ic * 3 + 1], cz = mesh.positions[ic * 3 + 2];
        float abx = bx - ax, aby = by - ay, abz = bz - az;
        float acx = cx - ax, acy = cy - ay, acz = cz - az;
        float nx = aby * acz - abz * acy;
        float ny = abz * acx - abx * acz;
        float nz = abx * acy - aby * acx;
        for (int vi : {ia, ib, ic}) {
            mesh.normals[vi * 3 + 0] += nx;
            mesh.normals[vi * 3 + 1] += ny;
            mesh.normals[vi * 3 + 2] += nz;
        }
    }
    for (size_t i = 0; i < mesh.normals.size(); i += 3) {
        float nx = mesh.normals[i], ny = mesh.normals[i + 1], nz = mesh.normals[i + 2];
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-6f) {
            mesh.normals[i] /= len;
            mesh.normals[i + 1] /= len;
            mesh.normals[i + 2] /= len;
        }
    }
}

static void LaplacianSmooth(MeshData& mesh, int iterations, float alpha) {
    if (iterations <= 0) return;
    const int vertexCount = (int)(mesh.positions.size() / 3);
    std::vector<std::vector<int>> neighbors(vertexCount);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int tri[3] = { mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2] };
        for (int a = 0; a < 3; ++a) {
            int v0 = tri[a];
            int v1 = tri[(a + 1) % 3];
            int v2 = tri[(a + 2) % 3];
            neighbors[v0].push_back(v1);
            neighbors[v0].push_back(v2);
        }
    }
    for (auto& n : neighbors) {
        std::sort(n.begin(), n.end());
        n.erase(std::unique(n.begin(), n.end()), n.end());
    }

    std::vector<float> src = mesh.positions;
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<float> dst = src;
        for (int v = 0; v < vertexCount; ++v) {
            if (neighbors[v].empty()) continue;
            float avg[3] = { 0, 0, 0 };
            for (int nb : neighbors[v]) {
                avg[0] += src[nb * 3 + 0];
                avg[1] += src[nb * 3 + 1];
                avg[2] += src[nb * 3 + 2];
            }
            const float inv = 1.0f / (float)neighbors[v].size();
            avg[0] *= inv; avg[1] *= inv; avg[2] *= inv;
            dst[v * 3 + 0] = src[v * 3 + 0] * (1.0f - alpha) + avg[0] * alpha;
            dst[v * 3 + 1] = src[v * 3 + 1] * (1.0f - alpha) + avg[1] * alpha;
            dst[v * 3 + 2] = src[v * 3 + 2] * (1.0f - alpha) + avg[2] * alpha;
        }
        src.swap(dst);
    }
    mesh.positions.swap(src);
    RecomputeNormals(mesh);
}

static bool EnsureDirectory(const fs::path& dir) {
    std::error_code ec;
    if (fs::exists(dir, ec)) return true;
    return fs::create_directories(dir, ec);
}
static bool CopyFilePortable(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    EnsureDirectory(to.parent_path());
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

static bool SaveBinaryPPM(const fs::path& path, int w, int h, const std::vector<unsigned char>& rgb) {
    EnsureDirectory(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << w << " " << h << "\n255\n";
    f.write(reinterpret_cast<const char*>(rgb.data()), (std::streamsize)rgb.size());
    return true;
}

static bool SaveMaskPreview(const fs::path& path, int w, int h, const std::vector<unsigned char>& mask) {
    std::vector<unsigned char> rgb((size_t)w * h * 3, 0);
    for (int i = 0; i < w * h; ++i) {
        unsigned char v = mask[(size_t)i] ? 255 : 0;
        rgb[(size_t)i * 3 + 0] = v;
        rgb[(size_t)i * 3 + 1] = v;
        rgb[(size_t)i * 3 + 2] = v;
    }
    return SaveBinaryPPM(path, w, h, rgb);
}

static bool SaveFrameContactSheet(const fs::path& path, const std::vector<ImageRGBA>& frames, int maxColumns) {
    if (frames.empty()) return false;
    int thumbW = std::min(256, frames.front().width);
    int thumbH = std::max(1, frames.front().height * thumbW / std::max(1, frames.front().width));
    int columns = std::min(maxColumns, (int)frames.size());
    int rows = ((int)frames.size() + columns - 1) / columns;
    int outW = columns * thumbW;
    int outH = rows * thumbH;
    std::vector<unsigned char> rgb((size_t)outW * outH * 3, 20);
    for (size_t fi = 0; fi < frames.size(); ++fi) {
        const auto& frame = frames[fi];
        int ox = (int)(fi % columns) * thumbW;
        int oy = (int)(fi / columns) * thumbH;
        for (int y = 0; y < thumbH; ++y) {
            int sy = y * frame.height / thumbH;
            for (int x = 0; x < thumbW; ++x) {
                int sx = x * frame.width / thumbW;
                size_t sp = ((size_t)sy * frame.width + sx) * 4;
                size_t dp = ((size_t)(oy + y) * outW + (ox + x)) * 3;
                rgb[dp + 0] = frame.pixels[sp + 0];
                rgb[dp + 1] = frame.pixels[sp + 1];
                rgb[dp + 2] = frame.pixels[sp + 2];
            }
        }
    }
    return SaveBinaryPPM(path, outW, outH, rgb);
}

static bool ExportOBJ(const MeshData& mesh, const fs::path& objPath, const std::string& textureFileName) {
    EnsureDirectory(objPath.parent_path());
    std::ofstream obj(objPath, std::ios::binary);
    if (!obj) return false;
    const std::string mtlName = objPath.stem().string() + ".mtl";
    obj << "mtllib " << mtlName << "\n" << "o Make3DMesh\n";
    for (size_t i = 0; i < mesh.positions.size(); i += 3) obj << "v " << mesh.positions[i] << ' ' << mesh.positions[i + 1] << ' ' << mesh.positions[i + 2] << "\n";
    for (size_t i = 0; i < mesh.uvs.size(); i += 2) obj << "vt " << mesh.uvs[i] << ' ' << (1.0f - mesh.uvs[i + 1]) << "\n";
    for (size_t i = 0; i < mesh.normals.size(); i += 3) obj << "vn " << mesh.normals[i] << ' ' << mesh.normals[i + 1] << ' ' << mesh.normals[i + 2] << "\n";
    obj << "usemtl Material0\n";
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        int a = mesh.indices[i] + 1, b = mesh.indices[i + 1] + 1, c = mesh.indices[i + 2] + 1;
        obj << "f " << a << '/' << a << '/' << a << ' ' << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << "\n";
    }
    std::ofstream mtl(objPath.parent_path() / mtlName, std::ios::binary);
    if (!mtl) return false;
    mtl << "newmtl Material0\nKa 1.000 1.000 1.000\nKd 1.000 1.000 1.000\nKs 0.000 0.000 0.000\n";
    if (!textureFileName.empty()) mtl << "map_Kd " << textureFileName << "\n";
    return true;
}


struct PortableConfig {
    bool skipModeDialog = false;
    bool defaultToVideoMode = false;
    bool skipPresetDialog = false;
    int targetFrameCount = 24;
    int voxelResolution = 84;
    float silhouetteThreshold = 34.0f;
    int morphologyPasses = 2;
    int meshSmoothIterations = 3;
    std::string outputFolder = "output";
};

static std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static bool ParseBool(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

static PortableConfig LoadPortableConfig() {
    PortableConfig cfg;
    std::vector<fs::path> candidates = {
        fs::current_path() / "config" / "portable_config.ini",
        fs::current_path() / "portable_config.ini"
    };
    for (const auto& path : candidates) {
        if (!fs::exists(path)) continue;
        std::ifstream in(path, std::ios::binary);
        if (!in) continue;
        std::string line;
        while (std::getline(in, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            if (key == "skip_mode_dialog") cfg.skipModeDialog = ParseBool(value);
            else if (key == "default_to_video_mode") cfg.defaultToVideoMode = ParseBool(value);
            else if (key == "skip_preset_dialog") cfg.skipPresetDialog = ParseBool(value);
            else if (key == "target_frame_count") cfg.targetFrameCount = std::max(8, std::atoi(value.c_str()));
            else if (key == "voxel_resolution") cfg.voxelResolution = std::max(32, std::atoi(value.c_str()));
            else if (key == "silhouette_threshold") cfg.silhouetteThreshold = std::max(1.0f, (float)std::atof(value.c_str()));
            else if (key == "morphology_passes") cfg.morphologyPasses = std::max(0, std::atoi(value.c_str()));
            else if (key == "mesh_smooth_iterations") cfg.meshSmoothIterations = std::max(0, std::atoi(value.c_str()));
            else if (key == "output_folder" && !value.empty()) cfg.outputFolder = value;
        }
        break;
    }
    return cfg;
}

static fs::path ResolveOutputDir(const PortableConfig& cfg) {
    fs::path out = fs::current_path() / cfg.outputFolder;
    EnsureDirectory(out);
    return out;
}

struct ComInit {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
};
struct MFInit {
    HRESULT hr = MFStartup(MF_VERSION);
    ~MFInit() { if (SUCCEEDED(hr)) MFShutdown(); }
};

template<typename T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

static VideoReconstructionSettings SelectVideoPreset() {
    VideoReconstructionSettings s;
    int choice = MessageBoxA(nullptr,
        "Video mode preset:\n\nYes  = High quality (slower, denser mesh)\nNo   = Fast preview (quicker, lighter mesh)\nCancel = Standard preset",
        "Make3D Portable", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDYES) {
        s.targetFrameCount = 32;
        s.voxelResolution = 96;
        s.silhouetteThreshold = 30.0f;
        s.morphologyPasses = 3;
        s.meshSmoothIterations = 4;
    } else if (choice == IDNO) {
        s.targetFrameCount = 16;
        s.voxelResolution = 64;
        s.silhouetteThreshold = 40.0f;
        s.morphologyPasses = 1;
        s.meshSmoothIterations = 1;
    }
    return s;
}

static bool ExtractVideoFrames(const std::string& path, int targetFrameCount, std::vector<ImageRGBA>& outFrames, std::string& reason) {
    ComInit com; if (FAILED(com.hr) && com.hr != RPC_E_CHANGED_MODE) { reason = "CoInitializeEx failed."; return false; }
    MFInit mf; if (FAILED(mf.hr)) { reason = "Media Foundation startup failed."; return false; }
    IMFAttributes* attrs = nullptr;
    IMFSourceReader* reader = nullptr;
    IMFMediaType* nativeType = nullptr;
    IMFMediaType* outType = nullptr;
    HRESULT hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr)) { reason = "MFCreateAttributes failed."; return false; }
    hr = MFCreateSourceReaderFromURL(std::wstring(path.begin(), path.end()).c_str(), attrs, &reader);
    SafeRelease(attrs);
    if (FAILED(hr)) { reason = "Failed to open video file. Use MP4/MOV/AVI/WMV on Windows 10/11."; return false; }
    hr = MFCreateMediaType(&outType);
    if (FAILED(hr)) { reason = "MFCreateMediaType failed."; SafeRelease(reader); return false; }
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
    SafeRelease(outType);
    if (FAILED(hr)) { reason = "Failed to request RGB32 video frames."; SafeRelease(reader); return false; }
    hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &nativeType);
    UINT32 width = 0, height = 0;
    if (SUCCEEDED(hr)) MFGetAttributeSize(nativeType, MF_MT_FRAME_SIZE, &width, &height);
    SafeRelease(nativeType);
    if (width == 0 || height == 0) { reason = "Could not determine video frame size."; SafeRelease(reader); return false; }

    std::vector<ImageRGBA> allFrames;
    const int maxFrames = std::max(12, std::min(targetFrameCount * 5, 300));
    while ((int)allFrames.size() < maxFrames) {
        DWORD flags = 0; IMFSample* sample = nullptr; IMFMediaBuffer* buffer = nullptr;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { SafeRelease(sample); break; }
        if (!sample) continue;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr) || !buffer) { SafeRelease(sample); continue; }
        BYTE* raw = nullptr; DWORD maxLen = 0, curLen = 0;
        hr = buffer->Lock(&raw, &maxLen, &curLen);
        if (SUCCEEDED(hr) && raw && curLen >= width * height * 4) {
            ImageRGBA img; img.width = (int)width; img.height = (int)height; img.pixels.resize((size_t)width * height * 4);
            for (UINT32 i = 0; i < width * height; ++i) {
                img.pixels[i * 4 + 0] = raw[i * 4 + 2];
                img.pixels[i * 4 + 1] = raw[i * 4 + 1];
                img.pixels[i * 4 + 2] = raw[i * 4 + 0];
                img.pixels[i * 4 + 3] = 255;
            }
            allFrames.push_back(std::move(img));
            buffer->Unlock();
        }
        SafeRelease(buffer); SafeRelease(sample);
    }
    SafeRelease(reader);
    if (allFrames.size() < 8) { reason = "Not enough frames could be decoded from the video."; return false; }

    outFrames.clear();
    int desired = std::min<int>(targetFrameCount, (int)allFrames.size());
    for (int i = 0; i < desired; ++i) {
        int idx = (int)std::llround((double)i * (double)(allFrames.size() - 1) / (double)std::max(1, desired - 1));
        outFrames.push_back(allFrames[idx]);
    }
    return true;
}

static std::array<float, 3> EstimateBackgroundColor(const ImageRGBA& frame) {
    auto sample = [&](int x, int y, int c) -> float { return frame.pixels[((size_t)y * frame.width + x) * 4 + c]; };
    std::array<float, 3> bg = { 0, 0, 0 };
    std::array<std::pair<int, int>, 8> points = {{
        {0, 0}, {frame.width - 1, 0}, {0, frame.height - 1}, {frame.width - 1, frame.height - 1},
        {frame.width / 2, 0}, {frame.width / 2, frame.height - 1}, {0, frame.height / 2}, {frame.width - 1, frame.height / 2}
    }};
    for (auto p : points) {
        for (int c = 0; c < 3; ++c) bg[c] += sample(p.first, p.second, c);
    }
    for (float& v : bg) v /= (float)points.size();
    return bg;
}

static std::vector<unsigned char> MorphologyMajority(const std::vector<unsigned char>& src, int w, int h, bool fillPass) {
    std::vector<unsigned char> dst = src;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int count = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (src[(size_t)(y + oy) * w + (x + ox)]) ++count;
                }
            }
            if (fillPass) dst[(size_t)y * w + x] = (count >= 4) ? 255 : 0;
            else dst[(size_t)y * w + x] = (count >= 6) ? 255 : 0;
        }
    }
    return dst;
}

static std::vector<unsigned char> BuildSilhouetteMask(const ImageRGBA& frame, float threshold, int morphologyPasses) {
    std::vector<unsigned char> mask((size_t)frame.width * frame.height, 0);
    auto bg = EstimateBackgroundColor(frame);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            size_t pi = ((size_t)y * frame.width + x) * 4;
            float dr = frame.pixels[pi + 0] - bg[0];
            float dg = frame.pixels[pi + 1] - bg[1];
            float db = frame.pixels[pi + 2] - bg[2];
            float dist = std::sqrt(dr * dr + dg * dg + db * db);
            mask[(size_t)y * frame.width + x] = dist > threshold ? 255 : 0;
        }
    }
    for (int i = 0; i < morphologyPasses; ++i) mask = MorphologyMajority(mask, frame.width, frame.height, true);
    for (int i = 0; i < std::max(1, morphologyPasses - 1); ++i) mask = MorphologyMajority(mask, frame.width, frame.height, false);
    return mask;
}

static void CleanVoxelVolume(std::vector<unsigned char>& vox, int nx, int ny, int nz, int passes) {
    auto vidx = [&](int x, int y, int z) { return (size_t)z * ny * nx + (size_t)y * nx + x; };
    for (int pass = 0; pass < passes; ++pass) {
        std::vector<unsigned char> src = vox;
        for (int z = 1; z < nz - 1; ++z) {
            for (int y = 1; y < ny - 1; ++y) {
                for (int x = 1; x < nx - 1; ++x) {
                    int neighbors = 0;
                    for (int oz = -1; oz <= 1; ++oz)
                        for (int oy = -1; oy <= 1; ++oy)
                            for (int ox = -1; ox <= 1; ++ox)
                                neighbors += src[vidx(x + ox, y + oy, z + oz)] ? 1 : 0;
                    if (src[vidx(x, y, z)]) vox[vidx(x, y, z)] = (neighbors >= 7) ? 1 : 0;
                    else vox[vidx(x, y, z)] = (neighbors >= 19) ? 1 : 0;
                }
            }
        }
    }
}

static MeshData BuildTurntableVisualHull(const std::vector<ImageRGBA>& frames, const VideoReconstructionSettings& settings, const fs::path& diagnosticsDir) {
    const int frameCount = (int)frames.size();
    const int w = frames.front().width;
    const int h = frames.front().height;
    std::vector<std::vector<unsigned char>> masks;
    masks.reserve(frames.size());
    for (size_t i = 0; i < frames.size(); ++i) {
        masks.push_back(BuildSilhouetteMask(frames[i], settings.silhouetteThreshold, settings.morphologyPasses));
        if (i < 6) {
            std::ostringstream name;
            name << "mask_" << i << ".ppm";
            SaveMaskPreview(diagnosticsDir / name.str(), w, h, masks.back());
        }
    }

    const int nx = settings.voxelResolution;
    const int ny = settings.voxelResolution;
    const int nz = settings.voxelResolution;
    std::vector<unsigned char> vox((size_t)nx * ny * nz, 1);
    auto vidx = [&](int x, int y, int z) { return (size_t)z * ny * nx + (size_t)y * nx + x; };

    for (int z = 0; z < nz; ++z) {
        float wz = ((float)z / (float)(nz - 1) - 0.5f) * 2.0f;
        for (int y = 0; y < ny; ++y) {
            float wy = ((float)y / (float)(ny - 1) - 0.5f) * 2.0f;
            for (int x = 0; x < nx; ++x) {
                float wx = ((float)x / (float)(nx - 1) - 0.5f) * 2.0f;
                bool inside = true;
                for (int fi = 0; fi < frameCount; ++fi) {
                    float theta = (float(fi) / (float)frameCount) * 6.28318530718f;
                    float rx = std::cos(theta) * wx + std::sin(theta) * wz;
                    float ry = wy;
                    int px = std::clamp((int)std::lround((rx * 0.5f + 0.5f) * (w - 1)), 0, w - 1);
                    int py = std::clamp((int)std::lround((0.5f - ry * 0.5f) * (h - 1)), 0, h - 1);
                    if (masks[fi][(size_t)py * w + px] == 0) { inside = false; break; }
                }
                vox[vidx(x, y, z)] = inside ? 1 : 0;
            }
        }
    }

    CleanVoxelVolume(vox, nx, ny, nz, 2);

    MeshData mesh;
    auto addVertex = [&](float x, float y, float z, float u, float v) -> int {
        int idx = (int)(mesh.positions.size() / 3);
        mesh.positions.insert(mesh.positions.end(), {x, y, z});
        mesh.normals.insert(mesh.normals.end(), {0, 0, 0});
        mesh.uvs.insert(mesh.uvs.end(), {u, v});
        return idx;
    };
    auto emitQuad = [&](float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float dx, float dy, float dz, float u0, float v0, float u1, float v1) {
        int i0 = addVertex(ax, ay, az, u0, v0), i1 = addVertex(bx, by, bz, u1, v0), i2 = addVertex(cx, cy, cz, u1, v1), i3 = addVertex(dx, dy, dz, u0, v1);
        AppendTri(mesh.indices, i0, i1, i2); AppendTri(mesh.indices, i0, i2, i3);
    };

    auto solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) return false;
        return vox[vidx(x, y, z)] != 0;
    };
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                if (!solid(x, y, z)) continue;
                float x0 = ((float)x / nx - 0.5f) * 2.0f;
                float x1 = ((float)(x + 1) / nx - 0.5f) * 2.0f;
                float y0 = ((float)y / ny - 0.5f) * 2.0f;
                float y1 = ((float)(y + 1) / ny - 0.5f) * 2.0f;
                float z0 = ((float)z / nz - 0.5f) * 2.0f;
                float z1 = ((float)(z + 1) / nz - 0.5f) * 2.0f;
                if (!solid(x - 1, y, z)) emitQuad(x0, y0, z1, x0, y0, z0, x0, y1, z0, x0, y1, z1, 0, 0, 1, 1);
                if (!solid(x + 1, y, z)) emitQuad(x1, y0, z0, x1, y0, z1, x1, y1, z1, x1, y1, z0, 0, 0, 1, 1);
                if (!solid(x, y - 1, z)) emitQuad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, 0, 0, 1, 1);
                if (!solid(x, y + 1, z)) emitQuad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, 0, 0, 1, 1);
                if (!solid(x, y, z - 1)) emitQuad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, 0, 0, 1, 1);
                if (!solid(x, y, z + 1)) emitQuad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, 0, 0, 1, 1);
            }
        }
    }
    RecomputeNormals(mesh);
    LaplacianSmooth(mesh, settings.meshSmoothIterations, 0.25f);
    return mesh;
}

static int RunPngDepthFusionMode(const PortableConfig& cfg) {
    std::vector<std::string> colorPaths = OpenMultiplePngFiles("Select one or more COLOR PNG files");
    if (colorPaths.empty()) { ShowError("No color images were selected."); return 1; }
    std::vector<std::string> depthPaths = OpenMultiplePngFiles("Select matching DEPTH PNG files in the same order");
    if (depthPaths.empty()) { ShowError("No depth images were selected."); return 1; }

    std::vector<ImageRGBA> colors; std::vector<DepthImage> depths;
    for (const auto& p : colorPaths) { auto img = LoadRGBA(p); if (!img) { ShowError("Failed to read color image:\n" + p); return 1; } colors.push_back(std::move(*img)); }
    for (const auto& p : depthPaths) { auto dep = LoadDepth(p); if (!dep) { ShowError("Failed to read depth image:\n" + p); return 1; } depths.push_back(std::move(*dep)); }
    std::string reason;
    if (!ValidatePairDimensions(colors, depths, reason)) { ShowError(reason); return 1; }
    const int w = colors.front().width, h = colors.front().height;
    std::vector<unsigned char> alphaMask = BuildAlphaMaskUnion(colors);
    std::vector<float> fused = FuseDepthMedianWeighted(depths);
    FillDepthHoles(fused, alphaMask, w, h);
    SmoothDepthEdgeAware(fused, alphaMask, w, h);
    MeshData mesh = BuildClosedDepthMesh(fused, alphaMask, w, h, 1.0f, 0.55f, 0.08f);
    RecomputeNormals(mesh);
    if (mesh.indices.empty()) { ShowError("Mesh generation failed. Check the alpha area in the PNG files."); return 1; }

    fs::path outputDir = ResolveOutputDir(cfg);
    fs::path textureDir = outputDir / "textures"; EnsureDirectory(textureDir);
    fs::path sourceTexture = colorPaths.front(); fs::path copiedTexture = textureDir / sourceTexture.filename();
    CopyFilePortable(sourceTexture, copiedTexture);
    fs::path objPath = outputDir / "model.obj";
    if (!ExportOBJ(mesh, objPath, (fs::path("textures") / sourceTexture.filename()).generic_string())) { ShowError("OBJ export failed."); return 1; }

    std::ofstream report(outputDir / "report.txt", std::ios::binary);
    if (report) {
        report << "Mode: Multi-image depth fusion\n";
        report << "Color images: " << colorPaths.size() << "\n";
        report << "Depth images: " << depthPaths.size() << "\n";
        report << "Resolution: " << w << "x" << h << "\n";
        report << "Vertices: " << (mesh.positions.size() / 3) << "\n";
        report << "Triangles: " << (mesh.indices.size() / 3) << "\n";
    }

    std::ostringstream summary;
    summary << "Export complete.\n\nMode: Multi-image depth fusion\nColor images: " << colorPaths.size() << "\nDepth images: " << depthPaths.size() << "\nVertices: " << (mesh.positions.size() / 3) << "\nTriangles: " << (mesh.indices.size() / 3) << "\n\nOBJ: " << objPath.string();
    ShowInfo(summary.str());
    return 0;
}

static int RunVideoTurntableMode(const PortableConfig& cfg) {
    auto videoPath = OpenSingleMediaFile("Select a turntable video (single file)");
    if (!videoPath) return 1;
    VideoReconstructionSettings settings = SelectVideoPreset();

    std::vector<ImageRGBA> frames;
    std::string reason;
    if (!ExtractVideoFrames(*videoPath, settings.targetFrameCount, frames, reason)) {
        ShowError("Video import failed.\n\n" + reason + "\n\nTips:\n- Use a turntable video\n- Keep the camera fixed\n- Use a plain background\n- Ensure the object stays centered");
        return 1;
    }

    fs::path outputDir = ResolveOutputDir(cfg);
    fs::path diagnosticsDir = outputDir / "diagnostics";
    EnsureDirectory(diagnosticsDir);
    SaveFrameContactSheet(diagnosticsDir / "frames_contact_sheet.ppm", frames, 4);

    MeshData mesh = BuildTurntableVisualHull(frames, settings, diagnosticsDir);
    if (mesh.indices.empty()) {
        ShowError("Video reconstruction did not produce a mesh. Try a cleaner background or more visible silhouette.");
        return 1;
    }

    fs::path objPath = outputDir / "turntable_visual_hull.obj";
    if (!ExportOBJ(mesh, objPath, "")) {
        ShowError("OBJ export failed.");
        return 1;
    }

    std::ofstream report(outputDir / "turntable_report.txt", std::ios::binary);
    if (report) {
        report << "Mode: Single-video turntable reconstruction\n";
        report << "Source video: " << *videoPath << "\n";
        report << "Frames used: " << frames.size() << "\n";
        report << "Voxel resolution: " << settings.voxelResolution << "\n";
        report << "Silhouette threshold: " << settings.silhouetteThreshold << "\n";
        report << "Morphology passes: " << settings.morphologyPasses << "\n";
        report << "Mesh smoothing iterations: " << settings.meshSmoothIterations << "\n";
        report << "Vertices: " << (mesh.positions.size() / 3) << "\n";
        report << "Triangles: " << (mesh.indices.size() / 3) << "\n";
        report << "Diagnostics: " << diagnosticsDir.string() << "\n";
    }

    std::ostringstream summary;
    summary << "Export complete.\n\nMode: Single-video turntable reconstruction\nFrames used: " << frames.size()
            << "\nVertices: " << (mesh.positions.size() / 3)
            << "\nTriangles: " << (mesh.indices.size() / 3)
            << "\n\nOBJ: " << objPath.string()
            << "\nDiagnostics: " << diagnosticsDir.string()
            << "\n\nThis mode assumes a fixed camera and a rotating object.";
    ShowInfo(summary.str());
    return 0;
}



struct GuiState {
    HWND hwnd = nullptr;
    HWND modeImages = nullptr;
    HWND modeVideo = nullptr;
    HWND colorEdit = nullptr;
    HWND depthEdit = nullptr;
    HWND videoEdit = nullptr;
    HWND presetCombo = nullptr;
    HWND frameEdit = nullptr;
    HWND voxelEdit = nullptr;
    HWND thresholdEdit = nullptr;
    HWND smoothEdit = nullptr;
    HWND outputEdit = nullptr;
    HWND chooseOutputButton = nullptr;
    HWND sampleButton = nullptr;
    HWND resetButton = nullptr;
    HWND runButton = nullptr;
    HWND openOutputButton = nullptr;
    HWND logEdit = nullptr;
    HWND statusLabel = nullptr;
    HWND progressBar = nullptr;
    HWND previewColor = nullptr;
    HWND previewDepth = nullptr;
    HWND previewVideo = nullptr;
    HBITMAP colorBitmap = nullptr;
    HBITMAP depthBitmap = nullptr;
    HBITMAP videoBitmap = nullptr;
    std::atomic<bool> isBusy = false;
    std::thread worker;
    int lastResultCode = 1;
    std::vector<std::string> colorPaths;
    std::vector<std::string> depthPaths;
    std::string videoPath;
    PortableConfig cfg;
};

static GuiState* g_gui = nullptr;

enum ControlId {
    IDC_MODE_IMAGES = 1001,
    IDC_MODE_VIDEO,
    IDC_COLOR_EDIT,
    IDC_COLOR_BROWSE,
    IDC_DEPTH_EDIT,
    IDC_DEPTH_BROWSE,
    IDC_VIDEO_EDIT,
    IDC_VIDEO_BROWSE,
    IDC_PRESET_COMBO,
    IDC_FRAMES_EDIT,
    IDC_VOXEL_EDIT,
    IDC_THRESHOLD_EDIT,
    IDC_SMOOTH_EDIT,
    IDC_OUTPUT_EDIT,
    IDC_OUTPUT_BROWSE,
    IDC_USE_SAMPLES,
    IDC_RESET_FORM,
    IDC_RUN,
    IDC_OPEN_OUTPUT,
    IDC_LOG,
    IDC_PROGRESS,
    IDC_STATUS,
    IDC_PREVIEW_COLOR,
    IDC_PREVIEW_DEPTH,
    IDC_PREVIEW_VIDEO
};

static constexpr UINT WM_APP_BUILD_DONE = WM_APP + 1;

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len ? len : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
static std::string ToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(len ? len : 0, '\0');
    if (len > 1) WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), len, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}
static void SetText(HWND hwnd, const std::string& text) {
    SetWindowTextW(hwnd, ToWide(text).c_str());
}
static std::string GetText(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring ws(len + 1, L'\0');
    GetWindowTextW(hwnd, ws.data(), len + 1);
    ws.resize(len);
    return ToUtf8(ws);
}
static void DestroyBitmap(HBITMAP& bmp) {
    if (bmp) {
        DeleteObject(bmp);
        bmp = nullptr;
    }
}

static HBITMAP CreatePreviewBitmap(const ImageRGBA& img, int targetW, int targetH) {
    if (img.width <= 0 || img.height <= 0 || targetW <= 0 || targetH <= 0) return nullptr;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = targetW;
    bi.bmiHeader.biHeight = -targetH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!bmp || !bits) return nullptr;
    auto* out = static_cast<unsigned char*>(bits);
    for (int y = 0; y < targetH; ++y) {
        int sy = y * img.height / targetH;
        for (int x = 0; x < targetW; ++x) {
            int sx = x * img.width / targetW;
            size_t sp = ((size_t)sy * img.width + sx) * 4;
            size_t dp = ((size_t)y * targetW + x) * 4;
            out[dp + 0] = img.pixels[sp + 2];
            out[dp + 1] = img.pixels[sp + 1];
            out[dp + 2] = img.pixels[sp + 0];
            out[dp + 3] = 255;
        }
    }
    return bmp;
}

static void ApplyBitmapToControl(HWND ctrl, HBITMAP& slot, HBITMAP bmp) {
    DestroyBitmap(slot);
    slot = bmp;
    SendMessageW(ctrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
}

static std::optional<std::string> BrowseForFolder(HWND owner, const std::wstring& title) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return std::nullopt;
    wchar_t path[MAX_PATH] = {};
    bool ok = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) return std::nullopt;
    return ToUtf8(path);
}

static fs::path FindProjectRoot() {
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    fs::path p = fs::path(modulePath).parent_path();
    for (int i = 0; i < 6; ++i) {
        if (fs::exists(p / "samples")) return p;
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    return fs::current_path();
}

static void ClearPreviewControl(HWND ctrl, HBITMAP& slot) {
    DestroyBitmap(slot);
    SendMessageW(ctrl, STM_SETIMAGE, IMAGE_BITMAP, 0);
}

static void SetStatusText(const std::string& text) {
    if (g_gui && g_gui->statusLabel) SetText(g_gui->statusLabel, text);
}

static void SetBusyUI(bool busy, const std::string& status) {
    if (!g_gui) return;
    g_gui->isBusy = busy;
    EnableWindow(g_gui->runButton, busy ? FALSE : TRUE);
    EnableWindow(g_gui->openOutputButton, busy ? FALSE : TRUE);
    EnableWindow(g_gui->modeImages, busy ? FALSE : TRUE);
    EnableWindow(g_gui->modeVideo, busy ? FALSE : TRUE);
    SetStatusText(status);
    ShowWindow(g_gui->progressBar, SW_SHOW);
    SendMessageW(g_gui->progressBar, PBM_SETMARQUEE, busy ? TRUE : FALSE, 0);
    if (!busy) SendMessageW(g_gui->progressBar, PBM_SETPOS, 0, 0);
}

static std::string JoinPreview(const std::vector<std::string>& files) {
    if (files.empty()) return "";
    if (files.size() == 1) return files.front();
    std::ostringstream oss;
    oss << files.size() << " files selected\n";
    for (size_t i = 0; i < std::min<size_t>(files.size(), 4); ++i) oss << "- " << files[i] << "\n";
    if (files.size() > 4) oss << "...";
    return oss.str();
}
static void AppendLog(const std::string& line) {
    if (!g_gui || !g_gui->logEdit) return;
    std::string current = GetText(g_gui->logEdit);
    current += line;
    if (!current.empty() && current.back() != '\n') current += '\n';
    SetText(g_gui->logEdit, current);
    SendMessageW(g_gui->logEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_gui->logEdit, EM_SCROLLCARET, 0, 0);
}
static bool IsVideoMode() {
    return SendMessageW(g_gui->modeVideo, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
static void RefreshModeUI();

static void AutoSwitchMode(bool video) {
    SendMessageW(g_gui->modeVideo, BM_SETCHECK, video ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_gui->modeImages, BM_SETCHECK, video ? BST_UNCHECKED : BST_CHECKED, 0);
    RefreshModeUI();
}

static void UpdateGuidance() {
    if (!g_gui) return;
    std::string status = "Ready";
    if (IsVideoMode()) {
        if (g_gui->videoPath.empty()) status = "Step 1: choose one turntable video.";
        else status = "Ready to build from video. Press Build 3D.";
    } else {
        if (g_gui->colorPaths.empty()) status = "Step 1: choose color PNG files.";
        else if (g_gui->depthPaths.empty()) status = "Step 2: choose matching depth PNG files.";
        else status = "Ready to build from PNG + depth files.";
    }
    SetStatusText(status);
}

static void RefreshModeUI() {
    bool video = IsVideoMode();
    EnableWindow(g_gui->colorEdit, !video);
    EnableWindow(GetDlgItem(g_gui->hwnd, IDC_COLOR_BROWSE), !video);
    EnableWindow(g_gui->depthEdit, !video);
    EnableWindow(GetDlgItem(g_gui->hwnd, IDC_DEPTH_BROWSE), !video);
    EnableWindow(g_gui->videoEdit, video);
    EnableWindow(GetDlgItem(g_gui->hwnd, IDC_VIDEO_BROWSE), video);
    EnableWindow(g_gui->presetCombo, video);
    EnableWindow(g_gui->frameEdit, video);
    EnableWindow(g_gui->voxelEdit, video);
    EnableWindow(g_gui->thresholdEdit, video);
    EnableWindow(g_gui->smoothEdit, video);
    ShowWindow(g_gui->previewVideo, video ? SW_SHOW : SW_HIDE);
    ShowWindow(g_gui->previewColor, video ? SW_HIDE : SW_SHOW);
    ShowWindow(g_gui->previewDepth, video ? SW_HIDE : SW_SHOW);
    UpdateGuidance();
}
static void ApplyPresetToControls(int presetIndex) {
    int frames = 24, voxel = 84, smooth = 3; float th = 34.0f;
    if (presetIndex == 0) { frames = 32; voxel = 96; th = 30.0f; smooth = 4; }
    else if (presetIndex == 2) { frames = 16; voxel = 64; th = 40.0f; smooth = 1; }
    SetText(g_gui->frameEdit, std::to_string(frames));
    SetText(g_gui->voxelEdit, std::to_string(voxel));
    SetText(g_gui->thresholdEdit, std::to_string(th));
    SetText(g_gui->smoothEdit, std::to_string(smooth));
}
static VideoReconstructionSettings ReadVideoSettingsFromUI() {
    VideoReconstructionSettings s;
    s.targetFrameCount = std::max(8, atoi(GetText(g_gui->frameEdit).c_str()));
    s.voxelResolution = std::max(32, atoi(GetText(g_gui->voxelEdit).c_str()));
    s.silhouetteThreshold = std::max(1.0f, (float)atof(GetText(g_gui->thresholdEdit).c_str()));
    s.meshSmoothIterations = std::max(0, atoi(GetText(g_gui->smoothEdit).c_str()));
    int preset = (int)SendMessageW(g_gui->presetCombo, CB_GETCURSEL, 0, 0);
    s.morphologyPasses = (preset == 0) ? 3 : (preset == 2 ? 1 : 2);
    return s;
}
static void OpenOutputFolder() {
    fs::path out = fs::current_path() / GetText(g_gui->outputEdit);
    EnsureDirectory(out);
    ShellExecuteW(nullptr, L"open", out.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
static int RunPngDepthFusionModeWithInputs(const PortableConfig& cfg, const std::vector<std::string>& colorPaths, const std::vector<std::string>& depthPaths) {
    std::vector<ImageRGBA> colors; std::vector<DepthImage> depths;
    for (const auto& p : colorPaths) { auto img = LoadRGBA(p); if (!img) { ShowError("Failed to read color image:\n" + p); return 1; } colors.push_back(std::move(*img)); }
    for (const auto& p : depthPaths) { auto dep = LoadDepth(p); if (!dep) { ShowError("Failed to read depth image:\n" + p); return 1; } depths.push_back(std::move(*dep)); }
    std::string reason;
    if (!ValidatePairDimensions(colors, depths, reason)) { ShowError(reason); return 1; }
    const int w = colors.front().width, h = colors.front().height;
    std::vector<unsigned char> alphaMask = BuildAlphaMaskUnion(colors);
    std::vector<float> fused = FuseDepthMedianWeighted(depths);
    FillDepthHoles(fused, alphaMask, w, h);
    SmoothDepthEdgeAware(fused, alphaMask, w, h);
    MeshData mesh = BuildClosedDepthMesh(fused, alphaMask, w, h, 1.0f, 0.55f, 0.08f);
    RecomputeNormals(mesh);
    if (mesh.indices.empty()) { ShowError("Mesh generation failed. Check the alpha area in the PNG files."); return 1; }

    fs::path outputDir = ResolveOutputDir(cfg);
    fs::path textureDir = outputDir / "textures"; EnsureDirectory(textureDir);
    fs::path sourceTexture = colorPaths.front(); fs::path copiedTexture = textureDir / sourceTexture.filename();
    CopyFilePortable(sourceTexture, copiedTexture);
    fs::path objPath = outputDir / "model.obj";
    if (!ExportOBJ(mesh, objPath, (fs::path("textures") / sourceTexture.filename()).generic_string())) { ShowError("OBJ export failed."); return 1; }

    std::ofstream report(outputDir / "report.txt", std::ios::binary);
    if (report) {
        report << "Mode: Multi-image depth fusion\n";
        report << "Color images: " << colorPaths.size() << "\n";
        report << "Depth images: " << depthPaths.size() << "\n";
        report << "Resolution: " << w << "x" << h << "\n";
        report << "Vertices: " << (mesh.positions.size() / 3) << "\n";
        report << "Triangles: " << (mesh.indices.size() / 3) << "\n";
    }
    return 0;
}
static int RunVideoTurntableModeWithInput(PortableConfig cfg, const std::string& videoPath, const VideoReconstructionSettings& settings) {
    std::vector<ImageRGBA> frames;
    std::string reason;
    if (!ExtractVideoFrames(videoPath, settings.targetFrameCount, frames, reason)) {
        ShowError("Video import failed.\n\n" + reason + "\n\nTips:\n- Use a turntable video\n- Keep the camera fixed\n- Use a plain background\n- Ensure the object stays centered");
        return 1;
    }
    fs::path outputDir = ResolveOutputDir(cfg);
    fs::path diagnosticsDir = outputDir / "diagnostics";
    EnsureDirectory(diagnosticsDir);
    SaveFrameContactSheet(diagnosticsDir / "frames_contact_sheet.ppm", frames, 4);
    MeshData mesh = BuildTurntableVisualHull(frames, settings, diagnosticsDir);
    if (mesh.indices.empty()) {
        ShowError("Video reconstruction did not produce a mesh. Try a cleaner background or more visible silhouette.");
        return 1;
    }
    fs::path objPath = outputDir / "turntable_visual_hull.obj";
    if (!ExportOBJ(mesh, objPath, "")) { ShowError("OBJ export failed."); return 1; }
    std::ofstream report(outputDir / "turntable_report.txt", std::ios::binary);
    if (report) {
        report << "Mode: Single-video turntable reconstruction\n";
        report << "Source video: " << videoPath << "\n";
        report << "Frames used: " << frames.size() << "\n";
        report << "Voxel resolution: " << settings.voxelResolution << "\n";
        report << "Silhouette threshold: " << settings.silhouetteThreshold << "\n";
        report << "Morphology passes: " << settings.morphologyPasses << "\n";
        report << "Mesh smoothing iterations: " << settings.meshSmoothIterations << "\n";
        report << "Vertices: " << (mesh.positions.size() / 3) << "\n";
        report << "Triangles: " << (mesh.indices.size() / 3) << "\n";
        report << "Diagnostics: " << diagnosticsDir.string() << "\n";
    }
    return 0;
}
static void UpdatePreviewFromInputs() {
    if (!g_gui) return;
    RECT rc{};
    GetClientRect(g_gui->previewColor, &rc);
    int pw = std::max<int>(1, static_cast<int>(rc.right - rc.left));
    int ph = std::max<int>(1, static_cast<int>(rc.bottom - rc.top));
    if (!g_gui->colorPaths.empty()) {
        auto img = LoadRGBA(g_gui->colorPaths.front());
        if (img) ApplyBitmapToControl(g_gui->previewColor, g_gui->colorBitmap, CreatePreviewBitmap(*img, pw, ph));
    } else {
        ClearPreviewControl(g_gui->previewColor, g_gui->colorBitmap);
    }
    if (!g_gui->depthPaths.empty()) {
        auto img = LoadRGBA(g_gui->depthPaths.front());
        if (img) ApplyBitmapToControl(g_gui->previewDepth, g_gui->depthBitmap, CreatePreviewBitmap(*img, pw, ph));
    } else {
        ClearPreviewControl(g_gui->previewDepth, g_gui->depthBitmap);
    }
    if (!g_gui->videoPath.empty()) {
        std::vector<ImageRGBA> frames; std::string reason;
        if (ExtractVideoFrames(g_gui->videoPath, 1, frames, reason) && !frames.empty()) {
            ApplyBitmapToControl(g_gui->previewVideo, g_gui->videoBitmap, CreatePreviewBitmap(frames.front(), pw, ph));
        } else {
            ClearPreviewControl(g_gui->previewVideo, g_gui->videoBitmap);
        }
    } else {
        ClearPreviewControl(g_gui->previewVideo, g_gui->videoBitmap);
    }
    UpdateGuidance();
}

static void StartGuiBuildAsync() {
    if (!g_gui || g_gui->isBusy) return;
    PortableConfig cfg = g_gui->cfg;
    cfg.outputFolder = GetText(g_gui->outputEdit);
    SetBusyUI(true, "Building 3D model...");
    AppendLog("Processing started...");
    if (g_gui->worker.joinable()) g_gui->worker.join();
    g_gui->worker = std::thread([cfg]() mutable {
        int rc = 1;
        if (IsVideoMode()) {
            VideoReconstructionSettings s = ReadVideoSettingsFromUI();
            rc = RunVideoTurntableModeWithInput(cfg, g_gui->videoPath, s);
        } else {
            rc = RunPngDepthFusionModeWithInputs(cfg, g_gui->colorPaths, g_gui->depthPaths);
        }
        g_gui->lastResultCode = rc;
        PostMessageW(g_gui->hwnd, WM_APP_BUILD_DONE, (WPARAM)rc, 0);
    });
}

static void RunFromGui() {
    PortableConfig cfg = g_gui->cfg;
    cfg.outputFolder = GetText(g_gui->outputEdit);
    if (IsVideoMode()) {
        if (g_gui->videoPath.empty()) {
            ShowError("Select a source video first.");
            return;
        }
        VideoReconstructionSettings s = ReadVideoSettingsFromUI();
        AppendLog("Mode: single video turntable reconstruction");
        AppendLog("Frames: " + std::to_string(s.targetFrameCount) + ", voxel resolution: " + std::to_string(s.voxelResolution));
    } else {
        if (g_gui->colorPaths.empty() || g_gui->depthPaths.empty()) {
            ShowError("Select both color PNG files and matching depth PNG files.");
            return;
        }
        AppendLog("Mode: multi-image PNG + depth fusion");
        AppendLog("Color files: " + std::to_string(g_gui->colorPaths.size()) + ", Depth files: " + std::to_string(g_gui->depthPaths.size()));
    }
    StartGuiBuildAsync();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        auto* gui = reinterpret_cast<GuiState*>(((CREATESTRUCTW*)lParam)->lpCreateParams);
        g_gui = gui;
        gui->hwnd = hwnd;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto addLabel=[&](int x,int y,int w,int h,const wchar_t* labelText){HWND ctrl=CreateWindowW(L"STATIC",labelText,WS_CHILD|WS_VISIBLE,x,y,w,h,hwnd,nullptr,nullptr,nullptr);SendMessageW(ctrl,WM_SETFONT,(WPARAM)font,TRUE);return ctrl;};
        auto addEdit=[&](int id,int x,int y,int w,int h,DWORD style=0){HWND ctrl=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|style,x,y,w,h,hwnd,(HMENU)(INT_PTR)id,nullptr,nullptr);SendMessageW(ctrl,WM_SETFONT,(WPARAM)font,TRUE);return ctrl;};
        auto addBtn=[&](int id,int x,int y,int w,int h,const wchar_t* buttonText,DWORD style=BS_PUSHBUTTON){HWND ctrl=CreateWindowW(L"BUTTON",buttonText,WS_CHILD|WS_VISIBLE|style,x,y,w,h,hwnd,(HMENU)(INT_PTR)id,nullptr,nullptr);SendMessageW(ctrl,WM_SETFONT,(WPARAM)font,TRUE);return ctrl;};
        addLabel(16,10,500,20,L"Make3D Portable - intuitive 2D to 3D tool");
        addLabel(16,30,640,18,L"1) Choose a mode  2) Add inputs  3) Adjust quality if needed  4) Click Build 3D");
        gui->modeImages = addBtn(IDC_MODE_IMAGES,18,58,260,20,L"Use color PNG files + matching depth PNG files",BS_AUTORADIOBUTTON);
        gui->modeVideo  = addBtn(IDC_MODE_VIDEO,300,58,250,20,L"Use one turntable video",BS_AUTORADIOBUTTON);
        SendMessageW(gui->modeVideo, BM_SETCHECK, BST_CHECKED, 0);

        addLabel(16,86,220,18,L"Color PNG files");
        addLabel(16,104,420,16,L"Tip: drag & drop files here, or click Browse.");
        gui->colorEdit = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,16,122,520,52,hwnd,(HMENU)IDC_COLOR_EDIT,nullptr,nullptr);
        SendMessageW(gui->colorEdit,WM_SETFONT,(WPARAM)font,TRUE);
        addBtn(IDC_COLOR_BROWSE,548,122,120,28,L"Browse...");

        addLabel(16,184,220,18,L"Matching depth PNG files");
        gui->depthEdit = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,16,208,520,52,hwnd,(HMENU)IDC_DEPTH_EDIT,nullptr,nullptr);
        SendMessageW(gui->depthEdit,WM_SETFONT,(WPARAM)font,TRUE);
        addBtn(IDC_DEPTH_BROWSE,548,208,120,28,L"Browse...");

        addLabel(16,272,220,18,L"Turntable video");
        addLabel(16,290,460,16,L"Best results: fixed camera, simple background, object centered.");
        gui->videoEdit = addEdit(IDC_VIDEO_EDIT,16,312,520,24,ES_READONLY);
        addBtn(IDC_VIDEO_BROWSE,548,310,120,28,L"Browse...");

        addLabel(16,350,200,18,L"Quality preset");
        gui->presetCombo = CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,16,372,160,200,hwnd,(HMENU)IDC_PRESET_COMBO,nullptr,nullptr);
        SendMessageW(gui->presetCombo,WM_SETFONT,(WPARAM)font,TRUE);
        SendMessageW(gui->presetCombo,CB_ADDSTRING,0,(LPARAM)L"High quality");
        SendMessageW(gui->presetCombo,CB_ADDSTRING,0,(LPARAM)L"Standard");
        SendMessageW(gui->presetCombo,CB_ADDSTRING,0,(LPARAM)L"Fast preview");
        SendMessageW(gui->presetCombo,CB_SETCURSEL,1,0);

        addLabel(200,350,80,18,L"Frames"); gui->frameEdit = addEdit(IDC_FRAMES_EDIT,200,372,70,24);
        addLabel(286,350,120,18,L"Voxel resolution"); gui->voxelEdit = addEdit(IDC_VOXEL_EDIT,286,372,90,24);
        addLabel(392,350,120,18,L"Threshold"); gui->thresholdEdit = addEdit(IDC_THRESHOLD_EDIT,392,372,70,24);
        addLabel(478,350,120,18,L"Smooth iters"); gui->smoothEdit = addEdit(IDC_SMOOTH_EDIT,478,372,58,24);

        addLabel(16,408,140,18,L"Output folder");
        gui->outputEdit = addEdit(IDC_OUTPUT_EDIT,16,430,300,24);
        gui->chooseOutputButton = addBtn(IDC_OUTPUT_BROWSE,324,428,92,28,L"Choose...");
        gui->openOutputButton = addBtn(IDC_OPEN_OUTPUT,428,428,110,28,L"Open output");
        gui->sampleButton = addBtn(IDC_USE_SAMPLES,548,428,120,28,L"Use samples");
        gui->resetButton = addBtn(IDC_RESET_FORM,548,462,120,28,L"Reset");
        gui->runButton = addBtn(IDC_RUN,548,496,120,36,L"Build 3D");

        addLabel(16,466,120,18,L"Status");
        gui->statusLabel = addEdit(IDC_STATUS,70,462,468,24,ES_READONLY);
        gui->progressBar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD|WS_VISIBLE|PBS_MARQUEE, 16, 496, 522, 20, hwnd, (HMENU)(INT_PTR)IDC_PROGRESS, nullptr, nullptr);
        SendMessageW(gui->progressBar, PBM_SETMARQUEE, FALSE, 0);

        addLabel(16,526,120,18,L"Preview");
        addLabel(16,546,120,18,L"Color");
        addLabel(236,546,120,18,L"Depth");
        addLabel(456,546,120,18,L"Video");
        gui->previewColor = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_BITMAP|SS_BLACKFRAME, 16, 566, 210, 118, hwnd, (HMENU)(INT_PTR)IDC_PREVIEW_COLOR, nullptr, nullptr);
        gui->previewDepth = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_BITMAP|SS_BLACKFRAME, 236, 566, 210, 118, hwnd, (HMENU)(INT_PTR)IDC_PREVIEW_DEPTH, nullptr, nullptr);
        gui->previewVideo = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_BITMAP|SS_BLACKFRAME, 456, 566, 212, 118, hwnd, (HMENU)(INT_PTR)IDC_PREVIEW_VIDEO, nullptr, nullptr);

        addLabel(16,694,280,18,L"Activity log and next-step hints");
        gui->logEdit = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,16,716,652,90,hwnd,(HMENU)(INT_PTR)IDC_LOG,nullptr,nullptr);
        SendMessageW(gui->logEdit,WM_SETFONT,(WPARAM)font,TRUE);

        SetText(gui->outputEdit, gui->cfg.outputFolder);
        ApplyPresetToControls(1);
        SetStatusText("Step 1: choose a mode.");
        AppendLog("Welcome to Make3D Portable.");
        AppendLog("Quick start: choose a mode, add files, then press Build 3D.");
        AppendLog("You can drag and drop PNG files or one video onto this window.");
        AppendLog("Tip: Use the bundled sample files if you want to test the app first.");
        DragAcceptFiles(hwnd, TRUE);
        RefreshModeUI();
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_MODE_IMAGES:
            AutoSwitchMode(false);
            AppendLog("Mode set to PNG + depth fusion.");
            break;
        case IDC_MODE_VIDEO:
            AutoSwitchMode(true);
            AppendLog("Mode set to single video turntable.");
            break;
        case IDC_COLOR_BROWSE:
            g_gui->colorPaths = OpenMultiplePngFiles("Select one or more COLOR PNG files");
            SetText(g_gui->colorEdit, JoinPreview(g_gui->colorPaths));
            if (!g_gui->colorPaths.empty()) { AutoSwitchMode(false); AppendLog("Selected color PNG files."); UpdatePreviewFromInputs(); }
            break;
        case IDC_DEPTH_BROWSE:
            g_gui->depthPaths = OpenMultiplePngFiles("Select matching DEPTH PNG files in the same order");
            SetText(g_gui->depthEdit, JoinPreview(g_gui->depthPaths));
            if (!g_gui->depthPaths.empty()) { AutoSwitchMode(false); AppendLog("Selected depth PNG files."); UpdatePreviewFromInputs(); }
            break;
        case IDC_VIDEO_BROWSE: {
            auto p = OpenSingleMediaFile("Select a turntable video (single file)");
            if (p) { g_gui->videoPath = *p; SetText(g_gui->videoEdit, g_gui->videoPath); AutoSwitchMode(true); AppendLog("Selected source video."); UpdatePreviewFromInputs(); }
            break; }
        case IDC_OUTPUT_BROWSE: {
            auto folder = BrowseForFolder(hwnd, L"Choose an output folder");
            if (folder) { SetText(g_gui->outputEdit, *folder); AppendLog("Updated output folder."); UpdateGuidance(); }
            break; }
        case IDC_USE_SAMPLES: {
            fs::path root = FindProjectRoot();
            fs::path samples = root / "samples";
            if (!fs::exists(samples / "input.png") || !fs::exists(samples / "depth.png")) {
                ShowError("Sample files were not found. Make sure the samples folder is bundled next to the app.");
                break;
            }
            g_gui->colorPaths = { (samples / "input.png").string() };
            g_gui->depthPaths = { (samples / "depth.png").string() };
            g_gui->videoPath.clear();
            SetText(g_gui->colorEdit, JoinPreview(g_gui->colorPaths));
            SetText(g_gui->depthEdit, JoinPreview(g_gui->depthPaths));
            SetText(g_gui->videoEdit, "");
            AutoSwitchMode(false);
            AppendLog("Loaded bundled sample PNG + depth files.");
            UpdatePreviewFromInputs();
            break; }
        case IDC_RESET_FORM:
            g_gui->colorPaths.clear();
            g_gui->depthPaths.clear();
            g_gui->videoPath.clear();
            SetText(g_gui->colorEdit, "");
            SetText(g_gui->depthEdit, "");
            SetText(g_gui->videoEdit, "");
            ClearPreviewControl(g_gui->previewColor, g_gui->colorBitmap);
            ClearPreviewControl(g_gui->previewDepth, g_gui->depthBitmap);
            ClearPreviewControl(g_gui->previewVideo, g_gui->videoBitmap);
            ApplyPresetToControls(1);
            AutoSwitchMode(true);
            AppendLog("Reset inputs and quality settings.");
            UpdateGuidance();
            break;
        case IDC_PRESET_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                ApplyPresetToControls((int)SendMessageW(g_gui->presetCombo, CB_GETCURSEL, 0, 0));
                AppendLog("Updated quality preset.");
            }
            break;
        case IDC_OPEN_OUTPUT:
            OpenOutputFolder();
            break;
        case IDC_RUN:
            RunFromGui();
            break;
        }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wParam;
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::string> pngs;
        std::vector<std::string> videos;
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH] = {};
            DragQueryFileW(drop, i, path, MAX_PATH);
            fs::path fp(path);
            std::wstring ext = fp.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            std::string utf = ToUtf8(fp.wstring());
            if (ext == L".png") pngs.push_back(utf);
            else if (ext == L".mp4" || ext == L".mov" || ext == L".avi" || ext == L".wmv") videos.push_back(utf);
        }
        DragFinish(drop);
        if (!videos.empty()) {
            g_gui->videoPath = videos.front();
            SetText(g_gui->videoEdit, g_gui->videoPath);
            AutoSwitchMode(true);
            AppendLog("Loaded video by drag and drop.");
        } else if (!pngs.empty()) {
            auto lower = [](std::string v){ std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return (char)std::tolower(c); }); return v; };
            std::sort(pngs.begin(), pngs.end());
            std::vector<std::string> colors, depths;
            for (const auto& p : pngs) {
                std::string n = lower(fs::path(p).filename().string());
                if (n.find("depth") != std::string::npos) depths.push_back(p);
                else colors.push_back(p);
            }
            if (!colors.empty()) { g_gui->colorPaths = colors; SetText(g_gui->colorEdit, JoinPreview(g_gui->colorPaths)); }
            if (!depths.empty()) { g_gui->depthPaths = depths; SetText(g_gui->depthEdit, JoinPreview(g_gui->depthPaths)); }
            AutoSwitchMode(false);
            AppendLog("Loaded PNG files by drag and drop.");
            if (!colors.empty() && depths.empty()) AppendLog("Hint: files with 'depth' in the name are assigned to the depth list automatically.");
        }
        UpdatePreviewFromInputs();
        return 0;
    }
    case WM_APP_BUILD_DONE:
        SetBusyUI(false, ((int)wParam == 0) ? "Build completed" : "Build failed");
        AppendLog(((int)wParam == 0) ? "Finished successfully. Open the output folder to inspect the OBJ and reports." : "Export failed. Check the message and try a simpler input.");
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_gui) {
            if (g_gui->worker.joinable()) g_gui->worker.join();
            DestroyBitmap(g_gui->colorBitmap);
            DestroyBitmap(g_gui->depthBitmap);
            DestroyBitmap(g_gui->videoBitmap);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);
    PortableConfig cfg = LoadPortableConfig();
    std::string cmd = lpCmdLine ? Trim(lpCmdLine) : std::string();
    if (cmd == "--video") return RunVideoTurntableMode(cfg);
    if (cmd == "--images") return RunPngDepthFusionMode(cfg);

    GuiState gui;
    gui.cfg = cfg;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Make3DGuiWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Make3D Portable GUI", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 700, 860, nullptr, nullptr, hInstance, &gui);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

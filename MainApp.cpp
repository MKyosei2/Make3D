#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 26819) // stb_image.h intentional fallthroughs
#pragma warning(disable: 6262)  // stb_image.h large stack usage in decoder internals
#include "stb_image.h"
#pragma warning(pop)

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")

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

struct PortableConfig {
    std::string outputFolder = "output";
};

enum class WorkflowMode {
    ImageDepth,
    Video
};

enum class QualityPreset {
    Easy,
    Recommended,
    Detailed
};

enum class ReconstructionPreset {
    Auto,
    Relief,
    PrimitiveBox,
    HumanoidProxy
};

struct AppState {
    HWND hwnd = nullptr;
    HWND lblTitle = nullptr;
    HWND lblSubtitle = nullptr;
    HWND grpWorkflow = nullptr;
    HWND btnModeImage = nullptr;
    HWND btnModeVideo = nullptr;
    HWND btnUseSamples = nullptr;
    HWND btnHelp = nullptr;

    HWND grpInput = nullptr;
    HWND btnAddColor = nullptr;
    HWND btnAddDepth = nullptr;
    HWND btnAddVideo = nullptr;
    HWND btnReset = nullptr;
    HWND listFiles = nullptr;

    HWND grpOutput = nullptr;
    HWND comboReconstruction = nullptr;
    HWND comboQuality = nullptr;
    HWND editOutput = nullptr;
    HWND btnChooseOutput = nullptr;
    HWND btnBuild = nullptr;
    HWND btnOpenOutput = nullptr;

    HWND grpPreview = nullptr;
    HWND lblInputPreview = nullptr;
    HWND lblModelPreview = nullptr;
    HWND imgInput = nullptr;
    HWND imgModel = nullptr;

    HWND lblReady = nullptr;
    HWND lblStatus = nullptr;
    HWND progress = nullptr;

    WorkflowMode mode = WorkflowMode::ImageDepth;
    QualityPreset quality = QualityPreset::Recommended;
    ReconstructionPreset reconstruction = ReconstructionPreset::Auto;
    PortableConfig cfg;
    std::vector<std::string> colorFiles;
    std::vector<std::string> depthFiles;
    std::optional<std::string> videoFile;
    std::optional<ImageRGBA> inputPreview;
    std::optional<MeshData> previewMesh;
    HBITMAP inputBitmap = nullptr;
    HBITMAP modelBitmap = nullptr;
    float previewYaw = 0.0f;
    std::atomic<bool> building = false;
};

static AppState g;
static const UINT WM_APP_BUILD_DONE = WM_APP + 10;
static const UINT TIMER_PREVIEW = 1;

struct BuildResult {
    bool ok = false;
    std::string message;
    fs::path objPath;
    std::optional<MeshData> previewMesh;
};

static std::optional<BuildResult> g_lastBuildResult;

static void UpdateUI();
static void UpdateInputPreviewBitmap();
static void UpdateModelPreviewBitmap();
static void ClearBitmap(HBITMAP& bmp);
static void SetStatusText(const std::string& text);
static void SetReadyText(const std::string& text);
static void ShowFriendlyError(const std::string& title, const std::string& body);
static void StartBuildAsync();
static ReconstructionPreset CurrentReconstructionModeFromUI();

static std::string Narrow(const std::wstring& ws) {
    if (ws.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), needed, nullptr, nullptr);
    return out;
}

static std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), needed);
    return out;
}

static std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
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
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            if (key == "output_folder" && !value.empty()) cfg.outputFolder = value;
        }
        break;
    }
    return cfg;
}

static fs::path ResolveOutputDir() {
    std::string text;
    int len = GetWindowTextLengthA(g.editOutput);
    if (len > 0) {
        std::vector<char> tmp((size_t)len + 1, '\0');
        GetWindowTextA(g.editOutput, tmp.data(), len + 1);
        text.assign(tmp.data());
    }
    text = Trim(text);
    fs::path out = text.empty() ? (fs::current_path() / g.cfg.outputFolder) : fs::path(text);
    std::error_code ec;
    fs::create_directories(out, ec);
    return out;
}

static void ClearBitmap(HBITMAP& bmp) {
    if (bmp) {
        DeleteObject(bmp);
        bmp = nullptr;
    }
}

static void ShowFriendlyError(const std::string& title, const std::string& body) {
    std::wstring wtitle = Widen(title);
    std::wstring wbody = Widen(body);
    MessageBoxW(g.hwnd, wbody.c_str(), wtitle.c_str(), MB_OK | MB_ICONERROR);
}

static void ShowInfo(const std::string& title, const std::string& body) {
    std::wstring wtitle = Widen(title);
    std::wstring wbody = Widen(body);
    MessageBoxW(g.hwnd, wbody.c_str(), wtitle.c_str(), MB_OK | MB_ICONINFORMATION);
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
    DepthImage img;
    img.width = w;
    img.height = h;
    img.values.resize((size_t)w * h, 0.0f);
    int step = std::max(1, comp);
    for (int i = 0; i < w * h; ++i) img.values[(size_t)i] = data[i * step] / 255.0f;
    stbi_image_free(data);
    return img;
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

static bool ExtractRepresentativeVideoFrame(const std::string& path, ImageRGBA& outImage, std::string& reason) {
    ComInit com;
    if (FAILED(com.hr) && com.hr != RPC_E_CHANGED_MODE) { reason = "動画の初期化に失敗しました。"; return false; }
    MFInit mf;
    if (FAILED(mf.hr)) { reason = "動画読み込み機能の初期化に失敗しました。"; return false; }

    IMFAttributes* attrs = nullptr;
    IMFSourceReader* reader = nullptr;
    IMFMediaType* outType = nullptr;
    HRESULT hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr)) { reason = "動画属性の作成に失敗しました。"; return false; }

    std::wstring widePath = Widen(path);
    hr = MFCreateSourceReaderFromURL(widePath.c_str(), attrs, &reader);
    SafeRelease(attrs);
    if (FAILED(hr)) { reason = "動画を開けませんでした。MP4 / MOV / AVI / WMV を試してください。"; return false; }

    hr = MFCreateMediaType(&outType);
    if (FAILED(hr)) { SafeRelease(reader); reason = "動画形式の作成に失敗しました。"; return false; }
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
    SafeRelease(outType);
    if (FAILED(hr)) { SafeRelease(reader); reason = "動画フレームを取り出せませんでした。"; return false; }

    for (int tries = 0; tries < 32; ++tries) {
        DWORD flags = 0;
        IMFSample* sample = nullptr;
        IMFMediaBuffer* buffer = nullptr;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) { SafeRelease(reader); reason = "動画フレームの読み込みに失敗しました。"; return false; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr) || !buffer) { SafeRelease(sample); continue; }
        BYTE* raw = nullptr; DWORD maxLen = 0; DWORD curLen = 0;
        hr = buffer->Lock(&raw, &maxLen, &curLen);
        if (SUCCEEDED(hr) && raw) {
            IMFMediaType* current = nullptr;
            UINT32 w = 0, h = 0;
            if (SUCCEEDED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current))) {
                MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &w, &h);
            }
            SafeRelease(current);

            const size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
            const size_t expectedBytes = (pixelCount <= (std::numeric_limits<size_t>::max)() / 4u) ? (pixelCount * 4u) : 0u;
            const size_t availableBytes = std::min<size_t>(static_cast<size_t>(curLen), static_cast<size_t>(maxLen));

            if (w > 0 && h > 0 && expectedBytes != 0u && availableBytes >= expectedBytes) {
                outImage.width = static_cast<int>(w);
                outImage.height = static_cast<int>(h);
                outImage.pixels.resize(expectedBytes);

                const BYTE* src = raw;
                const BYTE* const srcEnd = raw + expectedBytes;
                unsigned char* dst = outImage.pixels.data();
                while (src + 3 < srcEnd) {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    dst[3] = 255;
                    src += 4;
                    dst += 4;
                }

                buffer->Unlock();
                SafeRelease(buffer);
                SafeRelease(sample);
                SafeRelease(reader);
                return true;
            }
            buffer->Unlock();
        }
        SafeRelease(buffer);
        SafeRelease(sample);
    }
    SafeRelease(reader);
    reason = "動画からプレビュー用フレームを取り出せませんでした。";
    return false;
}

struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct BBox2i { int minX = 0, minY = 0, maxX = -1, maxY = -1; bool valid = false; };
struct ShapeAnalysis {
    BBox2i bbox;
    int area = 0;
    float rectangularity = 0.0f;
    float aspect = 1.0f;
    float headWidth = 0.0f;
    float shoulderWidth = 0.0f;
    float waistWidth = 0.0f;
    float hipWidth = 0.0f;
    float lowerGapRatio = 0.0f;
    float meanDepth = 0.0f;
    bool likelyHuman = false;
    bool likelyBox = false;
};

static Vec3 MakeVec3(float x, float y, float z) { return Vec3{x, y, z}; }
static Vec3 operator+(const Vec3& a, const Vec3& b) { return MakeVec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 operator-(const Vec3& a, const Vec3& b) { return MakeVec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static Vec3 operator*(const Vec3& a, float s) { return MakeVec3(a.x * s, a.y * s, a.z * s); }
static Vec3 operator/(const Vec3& a, float s) { return MakeVec3(a.x / s, a.y / s, a.z / s); }
static float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 Cross(const Vec3& a, const Vec3& b) { return MakeVec3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static float Length(const Vec3& v) { return std::sqrt(Dot(v,v)); }
static Vec3 Normalize(const Vec3& v) { float len = Length(v); return len > 1e-6f ? (v / len) : MakeVec3(0.0f,1.0f,0.0f); }

static int AddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    int idx = (int)(mesh.positions.size() / 3);
    mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(0.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u); mesh.uvs.push_back(v);
    return idx;
}

static void AppendMesh(MeshData& dst, const MeshData& src) {
    int base = (int)(dst.positions.size() / 3);
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    for (int idx : src.indices) dst.indices.push_back(base + idx);
}

static std::vector<unsigned char> BuildAlphaMaskUnion(const std::vector<ImageRGBA>& colors);

static std::vector<unsigned char> EstimateForegroundFromOpaqueImage(const ImageRGBA& image) {
    int w = image.width;
    int h = image.height;
    std::vector<unsigned char> mask((size_t)w * h, 0);
    if (w <= 0 || h <= 0 || image.pixels.size() < (size_t)w * h * 4) return mask;

    std::array<float, 3> bg = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> bgSq = {0.0f, 0.0f, 0.0f};
    int borderCount = 0;
    auto accumulatePixel = [&](int x, int y) {
        size_t idx = ((size_t)y * w + x) * 4;
        for (int c = 0; c < 3; ++c) {
            float v = (float)image.pixels[idx + c];
            bg[c] += v;
            bgSq[c] += v * v;
        }
        ++borderCount;
    };

    for (int x = 0; x < w; ++x) { accumulatePixel(x, 0); if (h > 1) accumulatePixel(x, h - 1); }
    for (int y = 1; y < h - 1; ++y) { accumulatePixel(0, y); if (w > 1) accumulatePixel(w - 1, y); }
    if (borderCount == 0) return mask;

    float stddev = 0.0f;
    for (int c = 0; c < 3; ++c) {
        bg[c] /= (float)borderCount;
        float meanSq = bgSq[c] / (float)borderCount;
        stddev += std::max(0.0f, meanSq - bg[c] * bg[c]);
    }
    stddev = std::sqrt(stddev / 3.0f);
    float threshold = std::max(22.0f, stddev * 2.4f + 14.0f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = ((size_t)y * w + x) * 4;
            float dr = (float)image.pixels[idx + 0] - bg[0];
            float dg = (float)image.pixels[idx + 1] - bg[1];
            float db = (float)image.pixels[idx + 2] - bg[2];
            float distance = std::sqrt(dr*dr + dg*dg + db*db);
            if (distance > threshold) mask[(size_t)y * w + x] = 255;
        }
    }

    auto copy = mask;
    for (int pass = 0; pass < 2; ++pass) {
        copy = mask;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int onCount = 0;
                for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) if (copy[(size_t)(y + oy) * w + (x + ox)] > 0) ++onCount;
                mask[(size_t)y * w + x] = (onCount >= 5) ? 255 : 0;
            }
        }
    }
    return mask;
}

static size_t CountMaskPixels(const std::vector<unsigned char>& mask) {
    size_t count = 0; for (auto v : mask) if (v) ++count; return count;
}

static std::vector<unsigned char> BuildForegroundMaskForColors(const std::vector<ImageRGBA>& colors) {
    auto mask = BuildAlphaMaskUnion(colors);
    const size_t maskCount = CountMaskPixels(mask);
    if (!colors.empty()) {
        const size_t total = (size_t)colors.front().width * colors.front().height;
        if (maskCount == 0 || maskCount > total * 98 / 100) {
            return EstimateForegroundFromOpaqueImage(colors.front());
        }
    }
    return mask;
}

static std::vector<unsigned char> BuildAlphaMaskUnion(const std::vector<ImageRGBA>& colors) {
    int w = colors.front().width;
    int h = colors.front().height;
    std::vector<unsigned char> mask((size_t)w * h, 0);
    for (const auto& img : colors) {
        for (int i = 0; i < w * h; ++i) {
            unsigned char a = img.pixels[(size_t)i * 4 + 3];
            if (a > 10) mask[(size_t)i] = 255;
        }
    }
    return mask;
}

static std::vector<unsigned char> BuildMaskFromVideoFrame(const ImageRGBA& frame) {
    int w = frame.width;
    int h = frame.height;
    std::array<float, 3> bg = {0, 0, 0};
    std::vector<POINT> samples = {
        { 4, 4 }, { std::max(0, w - 5), 4 }, { 4, std::max(0, h - 5) }, { std::max(0, w - 5), std::max(0, h - 5) }
    };
    for (const auto& p : samples) {
        size_t idx = ((size_t)p.y * w + p.x) * 4;
        bg[0] += frame.pixels[idx + 0];
        bg[1] += frame.pixels[idx + 1];
        bg[2] += frame.pixels[idx + 2];
    }
    bg[0] /= (float)samples.size(); bg[1] /= (float)samples.size(); bg[2] /= (float)samples.size();

    std::vector<unsigned char> mask((size_t)w * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = ((size_t)y * w + x) * 4;
            float dr = std::abs(frame.pixels[idx + 0] - bg[0]);
            float dg = std::abs(frame.pixels[idx + 1] - bg[1]);
            float db = std::abs(frame.pixels[idx + 2] - bg[2]);
            float diff = (dr + dg + db) / 3.0f;
            mask[(size_t)y * w + x] = diff > 22.0f ? 255 : 0;
        }
    }
    return mask;
}

static DepthImage BuildDepthFromLuminance(const ImageRGBA& img, const std::vector<unsigned char>& mask) {
    DepthImage depth;
    depth.width = img.width;
    depth.height = img.height;
    depth.values.resize((size_t)img.width * img.height, 0.0f);
    for (int i = 0; i < img.width * img.height; ++i) {
        size_t p = (size_t)i * 4;
        float lum = (img.pixels[p + 0] * 0.299f + img.pixels[p + 1] * 0.587f + img.pixels[p + 2] * 0.114f) / 255.0f;
        depth.values[(size_t)i] = mask[(size_t)i] ? lum : 0.0f;
    }
    return depth;
}

static bool ValidatePairDimensions(const std::vector<ImageRGBA>& colors, const std::vector<DepthImage>& depths, std::string& reason) {
    if (colors.empty()) { reason = "通常画像がまだ入っていません。"; return false; }
    if (depths.empty()) { reason = "深度画像がまだ入っていません。"; return false; }
    if (colors.size() != depths.size()) { reason = "通常画像と深度画像の枚数をそろえてください。"; return false; }
    int w = colors.front().width;
    int h = colors.front().height;
    for (size_t i = 0; i < colors.size(); ++i) {
        if (colors[i].width != w || colors[i].height != h) { reason = "通常画像の解像度がそろっていません。"; return false; }
        if (depths[i].width != w || depths[i].height != h) { reason = "深度画像の解像度が通常画像と一致していません。"; return false; }
    }
    return true;
}

static std::vector<float> FuseDepthMedianWeighted(const std::vector<DepthImage>& depths) {
    int w = depths.front().width;
    int h = depths.front().height;
    std::vector<float> out((size_t)w * h, 0.0f);
    std::vector<float> samples;
    samples.reserve(depths.size());
    for (int i = 0; i < w * h; ++i) {
        samples.clear();
        for (const auto& d : depths) samples.push_back(d.values[(size_t)i]);
        std::sort(samples.begin(), samples.end());
        float median = samples[samples.size() / 2];
        float sum = 0.0f;
        float wsum = 0.0f;
        for (float v : samples) {
            float weight = 1.0f - std::min(1.0f, std::abs(v - median) * 4.0f);
            weight = std::max(0.05f, weight);
            sum += v * weight;
            wsum += weight;
        }
        out[(size_t)i] = wsum > 0.0f ? sum / wsum : median;
    }
    return out;
}

static void FillDepthHoles(std::vector<float>& depth, const std::vector<unsigned char>& mask, int w, int h) {
    std::vector<float> src = depth;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int i = y * w + x;
            if (!mask[(size_t)i] || src[(size_t)i] > 0.001f) continue;
            float sum = 0.0f;
            int count = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    int ni = (y + oy) * w + (x + ox);
                    if (!mask[(size_t)ni]) continue;
                    if (src[(size_t)ni] <= 0.001f) continue;
                    sum += src[(size_t)ni];
                    ++count;
                }
            }
            if (count > 0) depth[(size_t)i] = sum / (float)count;
        }
    }
}

static void SmoothDepth(std::vector<float>& depth, const std::vector<unsigned char>& mask, int w, int h, int passes) {
    for (int pass = 0; pass < passes; ++pass) {
        std::vector<float> src = depth;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int i = y * w + x;
                if (!mask[(size_t)i]) continue;
                float center = src[(size_t)i];
                float sum = center * 2.0f;
                float wsum = 2.0f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!ox && !oy) continue;
                        int ni = (y + oy) * w + (x + ox);
                        if (!mask[(size_t)ni]) continue;
                        float diff = std::abs(src[(size_t)ni] - center);
                        float weight = diff < 0.08f ? 1.0f : 0.3f;
                        sum += src[(size_t)ni] * weight;
                        wsum += weight;
                    }
                }
                depth[(size_t)i] = sum / wsum;
            }
        }
    }
}

static void AppendTri(std::vector<int>& indices, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0) return;
    if (a == b || b == c || c == a) return;
    indices.push_back(a); indices.push_back(b); indices.push_back(c);
}

static bool IsValidVertexIndex(const MeshData& mesh, int index) {
    return index >= 0 && (size_t)index * 3 + 2 < mesh.positions.size();
}

static void RemoveInvalidTriangles(MeshData& mesh) {
    std::vector<int> cleaned;
    cleaned.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int a = mesh.indices[i + 0];
        int b = mesh.indices[i + 1];
        int c = mesh.indices[i + 2];
        if (!IsValidVertexIndex(mesh, a) || !IsValidVertexIndex(mesh, b) || !IsValidVertexIndex(mesh, c)) continue;
        if (a == b || b == c || c == a) continue;
        cleaned.push_back(a);
        cleaned.push_back(b);
        cleaned.push_back(c);
    }
    mesh.indices.swap(cleaned);
}

static MeshData BuildClosedDepthMesh(const std::vector<float>& depth, const std::vector<unsigned char>& mask, int w, int h, float xyScale, float depthScale, float thickness) {
    MeshData mesh;
    std::vector<int> front((size_t)w * h, -1);
    std::vector<int> back((size_t)w * h, -1);
    auto addVertex = [&](float x, float y, float z, float u, float v) {
        int idx = (int)(mesh.positions.size() / 3);
        mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
        mesh.normals.push_back(0); mesh.normals.push_back(0); mesh.normals.push_back(0);
        mesh.uvs.push_back(u); mesh.uvs.push_back(v);
        return idx;
    };
    float dw = (w > 1) ? (float)(w - 1) : 1.0f;
    float dh = (h > 1) ? (float)(h - 1) : 1.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int i = y * w + x;
            if (!mask[(size_t)i]) continue;
            float px = ((float)x / dw - 0.5f) * xyScale;
            float py = ((float)(h - 1 - y) / dh - 0.5f) * xyScale;
            float pz = depth[(size_t)i] * depthScale;
            float u = (float)x / dw;
            float v = (float)y / dh;
            front[(size_t)i] = addVertex(px, py, pz, u, v);
            back[(size_t)i] = addVertex(px, py, pz - thickness, u, v);
        }
    }
    for (int y = 0; y < h - 1; ++y) {
        for (int x = 0; x < w - 1; ++x) {
            int i00 = y * w + x;
            int i10 = y * w + x + 1;
            int i01 = (y + 1) * w + x;
            int i11 = (y + 1) * w + x + 1;
            if (front[(size_t)i00] >= 0 && front[(size_t)i10] >= 0 && front[(size_t)i11] >= 0) AppendTri(mesh.indices, front[(size_t)i00], front[(size_t)i10], front[(size_t)i11]);
            if (front[(size_t)i00] >= 0 && front[(size_t)i11] >= 0 && front[(size_t)i01] >= 0) AppendTri(mesh.indices, front[(size_t)i00], front[(size_t)i11], front[(size_t)i01]);
            if (back[(size_t)i00] >= 0 && back[(size_t)i11] >= 0 && back[(size_t)i10] >= 0) AppendTri(mesh.indices, back[(size_t)i00], back[(size_t)i11], back[(size_t)i10]);
            if (back[(size_t)i00] >= 0 && back[(size_t)i01] >= 0 && back[(size_t)i11] >= 0) AppendTri(mesh.indices, back[(size_t)i00], back[(size_t)i01], back[(size_t)i11]);
        }
    }
    auto sideQuad = [&](int a0, int a1, int b0, int b1) {
        if (a0 < 0 || a1 < 0 || b0 < 0 || b1 < 0) return;
        AppendTri(mesh.indices, a0, a1, b1);
        AppendTri(mesh.indices, a0, b1, b0);
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int i = y * w + x;
            if (!mask[(size_t)i]) continue;
            if (x == 0 || !mask[(size_t)(y * w + (x - 1))]) {
                int n = (y + 1 < h) ? ((y + 1) * w + x) : -1;
                sideQuad(front[(size_t)i], (n >= 0 ? front[(size_t)n] : -1), back[(size_t)i], (n >= 0 ? back[(size_t)n] : -1));
            }
            if (x == w - 1 || !mask[(size_t)(y * w + (x + 1))]) {
                int n = (y + 1 < h) ? ((y + 1) * w + x) : -1;
                if (n >= 0 && front[(size_t)i] >= 0 && back[(size_t)i] >= 0 && front[(size_t)n] >= 0 && back[(size_t)n] >= 0) {
                    AppendTri(mesh.indices, front[(size_t)i], back[(size_t)n], front[(size_t)n]);
                    AppendTri(mesh.indices, front[(size_t)i], back[(size_t)i], back[(size_t)n]);
                }
            }
            if (y == 0 || !mask[(size_t)((y - 1) * w + x)]) {
                int n = (x + 1 < w) ? (y * w + x + 1) : -1;
                sideQuad(front[(size_t)i], back[(size_t)i], (n >= 0 ? front[(size_t)n] : -1), (n >= 0 ? back[(size_t)n] : -1));
            }
            if (y == h - 1 || !mask[(size_t)((y + 1) * w + x)]) {
                int n = (x + 1 < w) ? (y * w + x + 1) : -1;
                if (n >= 0 && front[(size_t)i] >= 0 && back[(size_t)i] >= 0 && front[(size_t)n] >= 0 && back[(size_t)n] >= 0) {
                    AppendTri(mesh.indices, front[(size_t)i], front[(size_t)n], back[(size_t)n]);
                    AppendTri(mesh.indices, front[(size_t)i], back[(size_t)n], back[(size_t)i]);
                }
            }
        }
    }
    RemoveInvalidTriangles(mesh);
    return mesh;
}

static BBox2i ComputeMaskBBox(const std::vector<unsigned char>& mask, int w, int h) {
    BBox2i box;
    box.minX = w; box.minY = h; box.maxX = -1; box.maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[(size_t)y * w + x]) continue;
            box.valid = true;
            box.minX = std::min(box.minX, x); box.minY = std::min(box.minY, y);
            box.maxX = std::max(box.maxX, x); box.maxY = std::max(box.maxY, y);
        }
    }
    if (!box.valid) { box.minX = box.minY = 0; box.maxX = box.maxY = -1; }
    return box;
}

static std::vector<float> BuildRowWidths(const std::vector<unsigned char>& mask, int w, int h, const BBox2i& bbox) {
    std::vector<float> widths((size_t)h, 0.0f);
    if (!bbox.valid) return widths;
    for (int y = bbox.minY; y <= bbox.maxY; ++y) {
        int left = w, right = -1;
        for (int x = bbox.minX; x <= bbox.maxX; ++x) {
            if (!mask[(size_t)y * w + x]) continue;
            left = std::min(left, x);
            right = std::max(right, x);
        }
        if (right >= left) widths[(size_t)y] = (float)(right - left + 1);
    }
    std::vector<float> smooth = widths;
    for (int y = bbox.minY; y <= bbox.maxY; ++y) {
        float sum = 0.0f, weight = 0.0f;
        for (int oy = -2; oy <= 2; ++oy) {
            int yy = std::clamp(y + oy, bbox.minY, bbox.maxY);
            float wgt = (oy == 0) ? 2.0f : 1.0f;
            sum += widths[(size_t)yy] * wgt;
            weight += wgt;
        }
        smooth[(size_t)y] = weight > 0.0f ? sum / weight : widths[(size_t)y];
    }
    return smooth;
}

static float SampleRangeExtrema(const std::vector<float>& values, int start, int end, bool findMax) {
    if (values.empty()) return 0.0f;
    start = std::max(0, start); end = std::min((int)values.size() - 1, end);
    if (start > end) return 0.0f;
    float out = values[(size_t)start];
    for (int i = start + 1; i <= end; ++i) out = findMax ? std::max(out, values[(size_t)i]) : std::min(out, values[(size_t)i]);
    return out;
}

static ShapeAnalysis AnalyzeShape(const std::vector<float>& fusedDepth, const std::vector<unsigned char>& mask, int w, int h) {
    ShapeAnalysis analysis;
    analysis.bbox = ComputeMaskBBox(mask, w, h);
    if (!analysis.bbox.valid) return analysis;
    int bboxW = analysis.bbox.maxX - analysis.bbox.minX + 1;
    int bboxH = analysis.bbox.maxY - analysis.bbox.minY + 1;
    analysis.aspect = bboxW > 0 ? (float)bboxH / (float)bboxW : 1.0f;

    double depthSum = 0.0;
    for (int y = analysis.bbox.minY; y <= analysis.bbox.maxY; ++y) {
        for (int x = analysis.bbox.minX; x <= analysis.bbox.maxX; ++x) {
            size_t idx = (size_t)y * w + x;
            if (!mask[idx]) continue;
            ++analysis.area;
            depthSum += fusedDepth[idx];
        }
    }
    float bboxArea = (float)std::max(1, bboxW * bboxH);
    analysis.rectangularity = analysis.area / bboxArea;
    if (analysis.area > 0) analysis.meanDepth = (float)(depthSum / analysis.area);

    auto rowWidths = BuildRowWidths(mask, w, h, analysis.bbox);
    int top = analysis.bbox.minY, bottom = analysis.bbox.maxY, bodyH = std::max(1, bottom - top + 1);
    auto relY = [&](float t) { return top + std::clamp((int)std::round(t * (bodyH - 1)), 0, bodyH - 1); };
    analysis.headWidth = SampleRangeExtrema(rowWidths, relY(0.03f), relY(0.18f), true);
    analysis.shoulderWidth = SampleRangeExtrema(rowWidths, relY(0.18f), relY(0.34f), true);
    analysis.waistWidth = SampleRangeExtrema(rowWidths, relY(0.40f), relY(0.55f), false);
    analysis.hipWidth = SampleRangeExtrema(rowWidths, relY(0.55f), relY(0.72f), true);

    int gapY = relY(0.84f);
    int gap = 0, longestGap = 0, midX = (analysis.bbox.minX + analysis.bbox.maxX) / 2;
    for (int x = analysis.bbox.minX; x <= analysis.bbox.maxX; ++x) {
        bool inside = mask[(size_t)gapY * w + x] != 0;
        if (!inside) { ++gap; if (std::abs(x - midX) < bboxW / 4) longestGap = std::max(longestGap, gap); }
        else gap = 0;
    }
    analysis.lowerGapRatio = bboxW > 0 ? (float)longestGap / bboxW : 0.0f;

    bool proportionsHumanoid =
        analysis.aspect > 1.35f && analysis.aspect < 4.5f &&
        analysis.shoulderWidth > 0.0f && analysis.headWidth > 0.0f &&
        analysis.shoulderWidth > analysis.headWidth * 1.12f &&
        analysis.shoulderWidth > analysis.waistWidth * 1.08f &&
        analysis.hipWidth > analysis.waistWidth * 1.02f;
    analysis.likelyHuman = proportionsHumanoid;

    bool nearlySquare = bboxH > 0 && std::abs((float)bboxW / bboxH - 1.0f) < 0.18f;
    bool blockyRows = analysis.headWidth > 0.0f && analysis.hipWidth > 0.0f && std::abs(analysis.headWidth - analysis.hipWidth) < std::max(4.0f, analysis.headWidth * 0.08f);
    analysis.likelyBox = analysis.rectangularity > 0.94f && nearlySquare && blockyRows;
    return analysis;
}

static float PixelToWorldX(float x, int w, float xyScale) { float denomW = w > 1 ? (float)(w - 1) : 1.0f; return (x / denomW - 0.5f) * xyScale; }
static float PixelToWorldY(float y, int h, float xyScale) { float denomH = h > 1 ? (float)(h - 1) : 1.0f; return (((float)(h - 1) - y) / denomH - 0.5f) * xyScale; }
static float PixelWidthToWorld(float px, int w, float xyScale) { float denomW = w > 1 ? (float)(w - 1) : 1.0f; return px / denomW * xyScale; }
static float PixelHeightToWorld(float px, int h, float xyScale) { float denomH = h > 1 ? (float)(h - 1) : 1.0f; return px / denomH * xyScale; }

static void AppendQuad(MeshData& mesh, int a, int b, int c, int d) { AppendTri(mesh.indices, a, b, c); AppendTri(mesh.indices, a, c, d); }

static std::vector<float> BuildPseudoDepthFromMask(const std::vector<unsigned char>& mask, int w, int h) {
    const float kInf = 1.0e20f;
    std::vector<float> dist((size_t)w * h, 0.0f);
    if (mask.empty()) return dist;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (!mask[(size_t)idx]) { dist[(size_t)idx] = 0.0f; continue; }
            bool boundary = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
            if (!boundary) boundary = !mask[(size_t)(y * w + x - 1)] || !mask[(size_t)(y * w + x + 1)] || !mask[(size_t)((y - 1) * w + x)] || !mask[(size_t)((y + 1) * w + x)];
            dist[(size_t)idx] = boundary ? 0.0f : kInf;
        }
    }
    auto relax = [&](int x, int y, int nx, int ny, float cost) {
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) return;
        int idx = y * w + x; int nidx = ny * w + nx;
        if (!mask[(size_t)idx] || !mask[(size_t)nidx]) return;
        dist[(size_t)idx] = std::min(dist[(size_t)idx], dist[(size_t)nidx] + cost);
    };
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { relax(x,y,x-1,y,1.0f); relax(x,y,x,y-1,1.0f); relax(x,y,x-1,y-1,1.4142f); relax(x,y,x+1,y-1,1.4142f); }
    for (int y = h - 1; y >= 0; --y) for (int x = w - 1; x >= 0; --x) { relax(x,y,x+1,y,1.0f); relax(x,y,x,y+1,1.0f); relax(x,y,x+1,y+1,1.4142f); relax(x,y,x-1,y+1,1.4142f); }
    float maxDist = 0.0f;
    for (size_t i = 0; i < dist.size(); ++i) if (mask[i] && dist[i] < kInf) maxDist = std::max(maxDist, dist[i]);
    maxDist = std::max(maxDist, 1.0f);
    for (size_t i = 0; i < dist.size(); ++i) {
        if (!mask[i]) dist[i] = 0.0f;
        else { float t = std::max(0.0f, std::min(1.0f, dist[i] / maxDist)); dist[i] = std::pow(t, 0.85f); }
    }
    return dist;
}

static MeshData BuildBoxPrimitive(const ShapeAnalysis& analysis, int w, int h, float xyScale, float depthScale, float thickness) {
    MeshData mesh;
    if (!analysis.bbox.valid) return mesh;
    float x0 = PixelToWorldX((float)analysis.bbox.minX, w, xyScale);
    float x1 = PixelToWorldX((float)analysis.bbox.maxX, w, xyScale);
    float y0 = PixelToWorldY((float)analysis.bbox.maxY, h, xyScale);
    float y1 = PixelToWorldY((float)analysis.bbox.minY, h, xyScale);
    float sx = std::max(0.02f, x1 - x0);
    float sy = std::max(0.02f, y1 - y0);
    float sz = (sx + sy) * 0.5f;
    float aspect = sy > 1e-6f ? sx / sy : 1.0f;
    if (std::abs(aspect - 1.0f) > 0.12f) sz = std::max(thickness * 2.5f, std::min(sx, sy));
    float cx = (x0 + x1) * 0.5f; float cy = (y0 + y1) * 0.5f; float cz = std::max(analysis.meanDepth * depthScale, sz * 0.5f);
    float minX = cx - sx * 0.5f, maxX = cx + sx * 0.5f;
    float minY = cy - sy * 0.5f, maxY = cy + sy * 0.5f;
    float minZ = cz - sz * 0.5f, maxZ = cz + sz * 0.5f;
    auto face = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d) { int i0 = AddVertex(mesh,a.x,a.y,a.z,0,0); int i1 = AddVertex(mesh,b.x,b.y,b.z,1,0); int i2 = AddVertex(mesh,c.x,c.y,c.z,1,1); int i3 = AddVertex(mesh,d.x,d.y,d.z,0,1); AppendQuad(mesh,i0,i1,i2,i3); };
    face(MakeVec3(minX,minY,maxZ), MakeVec3(maxX,minY,maxZ), MakeVec3(maxX,maxY,maxZ), MakeVec3(minX,maxY,maxZ));
    face(MakeVec3(maxX,minY,minZ), MakeVec3(minX,minY,minZ), MakeVec3(minX,maxY,minZ), MakeVec3(maxX,maxY,minZ));
    face(MakeVec3(minX,minY,minZ), MakeVec3(minX,minY,maxZ), MakeVec3(minX,maxY,maxZ), MakeVec3(minX,maxY,minZ));
    face(MakeVec3(maxX,minY,maxZ), MakeVec3(maxX,minY,minZ), MakeVec3(maxX,maxY,minZ), MakeVec3(maxX,maxY,maxZ));
    face(MakeVec3(minX,maxY,maxZ), MakeVec3(maxX,maxY,maxZ), MakeVec3(maxX,maxY,minZ), MakeVec3(minX,maxY,minZ));
    face(MakeVec3(minX,minY,minZ), MakeVec3(maxX,minY,minZ), MakeVec3(maxX,minY,maxZ), MakeVec3(minX,minY,maxZ));
    return mesh;
}

static MeshData BuildEllipsoid(const Vec3& center, float rx, float ry, float rz, int slices, int stacks) {
    MeshData mesh; slices = std::max(6, slices); stacks = std::max(4, stacks);
    const float kPi = 3.14159265358979323846f;
    for (int iy = 0; iy <= stacks; ++iy) {
        float v = (float)iy / stacks; float phi = v * kPi; float sy = std::cos(phi); float sr = std::sin(phi);
        for (int ix = 0; ix <= slices; ++ix) {
            float u = (float)ix / slices; float theta = u * 2.0f * kPi; float cx = std::cos(theta); float cz = std::sin(theta);
            AddVertex(mesh, center.x + cx * sr * rx, center.y + sy * ry, center.z + cz * sr * rz, u, v);
        }
    }
    int row = slices + 1;
    for (int iy = 0; iy < stacks; ++iy) for (int ix = 0; ix < slices; ++ix) { int a = iy * row + ix; int b = a + 1; int c = a + row + 1; int d = a + row; AppendQuad(mesh,a,b,c,d); }
    return mesh;
}

static MeshData BuildCylinderBetween(const Vec3& a, const Vec3& b, float rx, float rz, int sides) {
    MeshData mesh; sides = std::max(6, sides); const float kPi = 3.14159265358979323846f;
    Vec3 axis = Normalize(b - a); Vec3 helper = std::abs(axis.y) > 0.95f ? MakeVec3(1,0,0) : MakeVec3(0,1,0);
    Vec3 tangent = Normalize(Cross(helper, axis)); Vec3 bitangent = Normalize(Cross(axis, tangent));
    std::vector<int> ringA; std::vector<int> ringB; ringA.reserve((size_t)sides); ringB.reserve((size_t)sides);
    for (int i = 0; i < sides; ++i) {
        float u = (float)i / sides; float ang = u * 2.0f * kPi; float c = std::cos(ang); float s = std::sin(ang);
        Vec3 offset = tangent * (c * rx) + bitangent * (s * rz);
        ringA.push_back(AddVertex(mesh, a.x + offset.x, a.y + offset.y, a.z + offset.z, u, 0.0f));
        ringB.push_back(AddVertex(mesh, b.x + offset.x, b.y + offset.y, b.z + offset.z, u, 1.0f));
    }
    for (int i = 0; i < sides; ++i) { int n = (i + 1) % sides; AppendQuad(mesh, ringA[i], ringA[n], ringB[n], ringB[i]); }
    int centerA = AddVertex(mesh, a.x, a.y, a.z, 0.5f, 0.5f); int centerB = AddVertex(mesh, b.x, b.y, b.z, 0.5f, 0.5f);
    for (int i = 0; i < sides; ++i) { int n = (i + 1) % sides; AppendTri(mesh.indices, centerA, ringA[n], ringA[i]); AppendTri(mesh.indices, centerB, ringB[i], ringB[n]); }
    return mesh;
}

static MeshData BuildHumanoidProxy(const ShapeAnalysis& analysis, int w, int h, float xyScale, float depthScale) {
    MeshData out; if (!analysis.bbox.valid) return out;
    int top = analysis.bbox.minY, bottom = analysis.bbox.maxY, left = analysis.bbox.minX, right = analysis.bbox.maxX;
    float bodyHpx = (float)(bottom - top + 1); float centerXPx = (left + right) * 0.5f; float centerX = PixelToWorldX(centerXPx, w, xyScale);
    float depthCenter = std::max(0.0f, analysis.meanDepth * depthScale);
    auto worldYAt = [&](float rel) { return PixelToWorldY(top + rel * (bodyHpx - 1.0f), h, xyScale); };
    auto widthToWorld = [&](float px) { return PixelWidthToWorld(px, w, xyScale); };
    float shoulderW = std::max(widthToWorld(analysis.shoulderWidth), widthToWorld((right - left + 1) * 0.42f));
    float waistW = std::max(widthToWorld(analysis.waistWidth), shoulderW * 0.58f);
    float hipW = std::max(widthToWorld(analysis.hipWidth), shoulderW * 0.72f);
    float headW = std::max(widthToWorld(analysis.headWidth), shoulderW * 0.36f);
    float worldH = PixelHeightToWorld(bodyHpx, h, xyScale);
    Vec3 headC = MakeVec3(centerX, worldYAt(0.10f), depthCenter + headW * 0.03f);
    Vec3 torsoC = MakeVec3(centerX, worldYAt(0.37f), depthCenter);
    Vec3 pelvisC = MakeVec3(centerX, worldYAt(0.60f), depthCenter - hipW * 0.03f);
    AppendMesh(out, BuildEllipsoid(headC, headW * 0.42f, worldH * 0.09f, headW * 0.34f, 18, 10));
    AppendMesh(out, BuildCylinderBetween(MakeVec3(centerX, worldYAt(0.18f), depthCenter), MakeVec3(centerX, worldYAt(0.24f), depthCenter), headW * 0.12f, headW * 0.10f, 12));
    AppendMesh(out, BuildEllipsoid(torsoC, shoulderW * 0.34f, worldH * 0.17f, shoulderW * 0.22f, 22, 12));
    AppendMesh(out, BuildEllipsoid(pelvisC, hipW * 0.31f, worldH * 0.11f, hipW * 0.22f, 20, 10));
    float shoulderY = worldYAt(0.26f), elbowY = worldYAt(0.47f), wristY = worldYAt(0.68f), shoulderOffset = shoulderW * 0.38f, armRadius = shoulderW * 0.08f;
    for (int side : {-1, 1}) {
        float sgn = (float)side; Vec3 shoulder = MakeVec3(centerX + sgn * shoulderOffset, shoulderY, depthCenter); Vec3 elbow = MakeVec3(centerX + sgn * shoulderW * 0.48f, elbowY, depthCenter); Vec3 wrist = MakeVec3(centerX + sgn * shoulderW * 0.42f, wristY, depthCenter);
        AppendMesh(out, BuildCylinderBetween(shoulder, elbow, armRadius, armRadius * 0.85f, 12));
        AppendMesh(out, BuildCylinderBetween(elbow, wrist, armRadius * 0.82f, armRadius * 0.72f, 12));
        AppendMesh(out, BuildEllipsoid(wrist, armRadius * 0.85f, armRadius * 0.95f, armRadius * 0.65f, 10, 8));
    }
    float legGap = std::max(hipW * (analysis.lowerGapRatio > 0.04f ? 0.20f : 0.12f), hipW * 0.10f);
    float hipY = worldYAt(0.66f), kneeY = worldYAt(0.84f), ankleY = worldYAt(0.98f), thighRadius = hipW * 0.12f, calfRadius = hipW * 0.09f;
    for (int side : {-1, 1}) {
        float sgn = (float)side; float x = centerX + sgn * legGap; Vec3 hip = MakeVec3(x, hipY, depthCenter - hipW * 0.02f); Vec3 knee = MakeVec3(x, kneeY, depthCenter); Vec3 ankle = MakeVec3(x, ankleY, depthCenter + hipW * 0.03f);
        AppendMesh(out, BuildCylinderBetween(hip, knee, thighRadius, thighRadius * 0.92f, 14));
        AppendMesh(out, BuildCylinderBetween(knee, ankle, calfRadius, calfRadius * 0.84f, 14));
        AppendMesh(out, BuildEllipsoid(ankle + MakeVec3(0.0f, -worldH * 0.012f, hipW * 0.05f), calfRadius * 1.25f, worldH * 0.025f, calfRadius * 1.7f, 12, 8));
    }
    RemoveInvalidTriangles(out);
    return out;
}

static MeshData BuildSemanticMesh(const std::vector<float>& depth, const std::vector<unsigned char>& mask, int w, int h, float xyScale, float depthScale, float thickness, ReconstructionPreset preset) {
    std::vector<float> usableDepth = depth;
    bool hasDepth = usableDepth.size() == mask.size();
    float minDepth = 1.0f, maxDepth = 0.0f;
    if (hasDepth) {
        bool seen = false;
        for (size_t i = 0; i < usableDepth.size(); ++i) {
            if (!mask[i]) continue;
            minDepth = std::min(minDepth, usableDepth[i]);
            maxDepth = std::max(maxDepth, usableDepth[i]);
            seen = true;
        }
        hasDepth = seen && (maxDepth - minDepth) >= 0.02f;
    }
    if (!hasDepth) usableDepth = BuildPseudoDepthFromMask(mask, w, h);
    ShapeAnalysis analysis = AnalyzeShape(usableDepth, mask, w, h);
    ReconstructionPreset resolved = preset;
    if (resolved == ReconstructionPreset::Auto) {
        if (analysis.likelyBox) resolved = ReconstructionPreset::PrimitiveBox;
        else if (analysis.likelyHuman) resolved = ReconstructionPreset::HumanoidProxy;
        else resolved = ReconstructionPreset::Relief;
    }
    if (resolved == ReconstructionPreset::PrimitiveBox) return BuildBoxPrimitive(analysis, w, h, xyScale, depthScale, thickness);
    if (resolved == ReconstructionPreset::HumanoidProxy) return BuildHumanoidProxy(analysis, w, h, xyScale, depthScale);
    return BuildClosedDepthMesh(usableDepth, mask, w, h, xyScale, depthScale, thickness);
}

static void RecomputeNormals(MeshData& mesh) {
    std::fill(mesh.normals.begin(), mesh.normals.end(), 0.0f);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        if (!IsValidVertexIndex(mesh, ia) || !IsValidVertexIndex(mesh, ib) || !IsValidVertexIndex(mesh, ic)) continue;
        float ax = mesh.positions[(size_t)ia * 3 + 0], ay = mesh.positions[(size_t)ia * 3 + 1], az = mesh.positions[(size_t)ia * 3 + 2];
        float bx = mesh.positions[(size_t)ib * 3 + 0], by = mesh.positions[(size_t)ib * 3 + 1], bz = mesh.positions[(size_t)ib * 3 + 2];
        float cx = mesh.positions[(size_t)ic * 3 + 0], cy = mesh.positions[(size_t)ic * 3 + 1], cz = mesh.positions[(size_t)ic * 3 + 2];
        float abx = bx - ax, aby = by - ay, abz = bz - az;
        float acx = cx - ax, acy = cy - ay, acz = cz - az;
        float nx = aby * acz - abz * acy;
        float ny = abz * acx - abx * acz;
        float nz = abx * acy - aby * acx;
        for (int vi : { ia, ib, ic }) {
            mesh.normals[(size_t)vi * 3 + 0] += nx;
            mesh.normals[(size_t)vi * 3 + 1] += ny;
            mesh.normals[(size_t)vi * 3 + 2] += nz;
        }
    }
    for (size_t i = 0; i < mesh.normals.size(); i += 3) {
        float nx = mesh.normals[i], ny = mesh.normals[i + 1], nz = mesh.normals[i + 2];
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-6f) {
            mesh.normals[i] /= len; mesh.normals[i + 1] /= len; mesh.normals[i + 2] /= len;
        }
    }
}

static void LaplacianSmooth(MeshData& mesh, int iterations, float alpha) {
    int count = (int)(mesh.positions.size() / 3);
    if (iterations <= 0 || count <= 0) return;
    std::vector<std::vector<int>> neighbors((size_t)count);
    RemoveInvalidTriangles(mesh);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (!IsValidVertexIndex(mesh, a) || !IsValidVertexIndex(mesh, b) || !IsValidVertexIndex(mesh, c)) continue;
        neighbors[(size_t)a].push_back(b); neighbors[(size_t)a].push_back(c);
        neighbors[(size_t)b].push_back(a); neighbors[(size_t)b].push_back(c);
        neighbors[(size_t)c].push_back(a); neighbors[(size_t)c].push_back(b);
    }
    for (auto& n : neighbors) {
        std::sort(n.begin(), n.end());
        n.erase(std::unique(n.begin(), n.end()), n.end());
    }
    std::vector<float> src = mesh.positions;
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<float> dst = src;
        for (int v = 0; v < count; ++v) {
            if (neighbors[(size_t)v].empty()) continue;
            float avgX = 0, avgY = 0, avgZ = 0;
            for (int nb : neighbors[(size_t)v]) {
                avgX += src[(size_t)nb * 3 + 0];
                avgY += src[(size_t)nb * 3 + 1];
                avgZ += src[(size_t)nb * 3 + 2];
            }
            float inv = 1.0f / (float)neighbors[(size_t)v].size();
            avgX *= inv; avgY *= inv; avgZ *= inv;
            dst[(size_t)v * 3 + 0] = src[(size_t)v * 3 + 0] * (1.0f - alpha) + avgX * alpha;
            dst[(size_t)v * 3 + 1] = src[(size_t)v * 3 + 1] * (1.0f - alpha) + avgY * alpha;
            dst[(size_t)v * 3 + 2] = src[(size_t)v * 3 + 2] * (1.0f - alpha) + avgZ * alpha;
        }
        src.swap(dst);
    }
    mesh.positions.swap(src);
    RecomputeNormals(mesh);
}

static bool SaveBinaryPPM(const fs::path& path, int w, int h, const std::vector<unsigned char>& rgb) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << w << ' ' << h << "\n255\n";
    f.write(reinterpret_cast<const char*>(rgb.data()), (std::streamsize)rgb.size());
    return true;
}

static bool ExportOBJ(const MeshData& mesh, const fs::path& objPath, const std::string& textureName) {
    std::error_code ec;
    fs::create_directories(objPath.parent_path(), ec);
    std::ofstream obj(objPath, std::ios::binary);
    if (!obj) return false;
    std::string mtlName = objPath.stem().string() + ".mtl";
    obj << "mtllib " << mtlName << "\n";
    obj << "o Make3DModel\n";
    for (size_t i = 0; i < mesh.positions.size(); i += 3) obj << "v " << mesh.positions[i] << ' ' << mesh.positions[i + 1] << ' ' << mesh.positions[i + 2] << "\n";
    for (size_t i = 0; i < mesh.uvs.size(); i += 2) obj << "vt " << mesh.uvs[i] << ' ' << (1.0f - mesh.uvs[i + 1]) << "\n";
    for (size_t i = 0; i < mesh.normals.size(); i += 3) obj << "vn " << mesh.normals[i] << ' ' << mesh.normals[i + 1] << ' ' << mesh.normals[i + 2] << "\n";
    obj << "usemtl Material0\n";
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        int ia = mesh.indices[i + 0], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        if (!IsValidVertexIndex(mesh, ia) || !IsValidVertexIndex(mesh, ib) || !IsValidVertexIndex(mesh, ic)) continue;
        int a = ia + 1, b = ib + 1, c = ic + 1;
        obj << "f " << a << '/' << a << '/' << a << ' ' << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << "\n";
    }
    std::ofstream mtl(objPath.parent_path() / mtlName, std::ios::binary);
    if (!mtl) return false;
    mtl << "newmtl Material0\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\n";
    if (!textureName.empty()) mtl << "map_Kd " << textureName << "\n";
    return true;
}

static HBITMAP CreateBitmapFromRGBA(const ImageRGBA& img, int targetW, int targetH) {
    if (img.width <= 0 || img.height <= 0 || targetW <= 0 || targetH <= 0) return nullptr;
    std::vector<unsigned char> scaled((size_t)targetW * targetH * 4, 255);
    for (int y = 0; y < targetH; ++y) {
        int sy = y * img.height / targetH;
        for (int x = 0; x < targetW; ++x) {
            int sx = x * img.width / targetW;
            size_t sp = ((size_t)sy * img.width + sx) * 4;
            size_t dp = ((size_t)y * targetW + x) * 4;
            scaled[dp + 0] = img.pixels[sp + 2];
            scaled[dp + 1] = img.pixels[sp + 1];
            scaled[dp + 2] = img.pixels[sp + 0];
            scaled[dp + 3] = 255;
        }
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = targetW;
    bi.bmiHeader.biHeight = -targetH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = GetDC(g.hwnd);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(g.hwnd, dc);
    if (!bmp || !bits) return nullptr;
    std::memcpy(bits, scaled.data(), scaled.size());
    return bmp;
}

static HBITMAP RenderMeshPreviewBitmap(const MeshData& mesh, int width, int height, float yaw) {
    if (width <= 0 || height <= 0 || mesh.positions.empty()) return nullptr;
    struct V3 { float x, y, z; };
    std::vector<V3> transformed(mesh.positions.size() / 3);
    float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
    float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float cx = std::cos(-0.45f);
    float sx = std::sin(-0.45f);
    for (size_t i = 0; i < transformed.size(); ++i) {
        float x = mesh.positions[i * 3 + 0];
        float y = mesh.positions[i * 3 + 1];
        float z = mesh.positions[i * 3 + 2];
        float rx = x * cy + z * sy;
        float rz = -x * sy + z * cy;
        float ry = y * cx - rz * sx;
        float rz2 = y * sx + rz * cx;
        transformed[i] = { rx, ry, rz2 };
        minX = std::min(minX, rx); minY = std::min(minY, ry); minZ = std::min(minZ, rz2);
        maxX = std::max(maxX, rx); maxY = std::max(maxY, ry); maxZ = std::max(maxZ, rz2);
    }
    float spanX = std::max(0.001f, maxX - minX);
    float spanY = std::max(0.001f, maxY - minY);
    float scale = 0.78f * std::min((float)width / spanX, (float)height / spanY);
    std::vector<unsigned char> pixels((size_t)width * height * 4, 0);
    std::vector<float> zbuf((size_t)width * height, 1e30f);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 248; pixels[i + 1] = 248; pixels[i + 2] = 248; pixels[i + 3] = 255;
    }
    auto putPixel = [&](int x, int y, float z, unsigned char shade) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        size_t idx = (size_t)y * width + x;
        if (z >= zbuf[idx]) return;
        zbuf[idx] = z;
        pixels[idx * 4 + 0] = shade;
        pixels[idx * 4 + 1] = shade;
        pixels[idx * 4 + 2] = shade;
        pixels[idx * 4 + 3] = 255;
    };
    auto edge = [](float ax, float ay, float bx, float by, float cx, float cy) {
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
    };
    std::array<float, 3> light = { 0.35f, 0.45f, 0.82f };
    float llen = std::sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    for (float& c : light) c /= llen;

    for (size_t ti = 0; ti + 2 < mesh.indices.size(); ti += 3) {
        int ia = mesh.indices[ti + 0];
        int ib = mesh.indices[ti + 1];
        int ic = mesh.indices[ti + 2];
        if (!IsValidVertexIndex(mesh, ia) || !IsValidVertexIndex(mesh, ib) || !IsValidVertexIndex(mesh, ic)) continue;
        V3 a = transformed[(size_t)ia];
        V3 b = transformed[(size_t)ib];
        V3 c = transformed[(size_t)ic];
        float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
        float acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;
        float nx = aby * acz - abz * acy;
        float ny = abz * acx - abx * acz;
        float nz = abx * acy - aby * acx;
        float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen < 1e-6f) continue;
        nx /= nlen; ny /= nlen; nz /= nlen;
        if (nz >= 0.0f) continue;
        float lit = std::max(0.0f, nx * light[0] + ny * light[1] + nz * light[2]);
        unsigned char shade = (unsigned char)std::clamp(80.0f + lit * 140.0f, 0.0f, 255.0f);

        float sx0 = (a.x - (minX + maxX) * 0.5f) * scale + width * 0.5f;
        float sy0 = height * 0.5f - (a.y - (minY + maxY) * 0.5f) * scale;
        float sx1 = (b.x - (minX + maxX) * 0.5f) * scale + width * 0.5f;
        float sy1 = height * 0.5f - (b.y - (minY + maxY) * 0.5f) * scale;
        float sx2 = (c.x - (minX + maxX) * 0.5f) * scale + width * 0.5f;
        float sy2 = height * 0.5f - (c.y - (minY + maxY) * 0.5f) * scale;

        int minPx = std::max(0, (int)std::floor(std::min({ sx0, sx1, sx2 })));
        int maxPx = std::min(width - 1, (int)std::ceil(std::max({ sx0, sx1, sx2 })));
        int minPy = std::max(0, (int)std::floor(std::min({ sy0, sy1, sy2 })));
        int maxPy = std::min(height - 1, (int)std::ceil(std::max({ sy0, sy1, sy2 })));
        float area = edge(sx0, sy0, sx1, sy1, sx2, sy2);
        if (std::abs(area) < 1e-6f) continue;
        for (int y = minPy; y <= maxPy; ++y) {
            for (int x = minPx; x <= maxPx; ++x) {
                float px = x + 0.5f;
                float py = y + 0.5f;
                float w0 = edge(sx1, sy1, sx2, sy2, px, py) / area;
                float w1 = edge(sx2, sy2, sx0, sy0, px, py) / area;
                float w2 = edge(sx0, sy0, sx1, sy1, px, py) / area;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                float z = a.z * w0 + b.z * w1 + c.z * w2;
                putPixel(x, y, z, shade);
            }
        }
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = GetDC(g.hwnd);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(g.hwnd, dc);
    if (!bmp || !bits) return nullptr;
    std::memcpy(bits, pixels.data(), pixels.size());
    return bmp;
}

static void UpdateInputPreviewBitmap() {
    ClearBitmap(g.inputBitmap);
    if (!g.inputPreview) {
        SendMessage(g.imgInput, STM_SETIMAGE, IMAGE_BITMAP, 0);
        return;
    }
    RECT rc{};
    GetClientRect(g.imgInput, &rc);
    int w = std::max<int>(10, static_cast<int>(rc.right - rc.left));
    int h = std::max<int>(10, static_cast<int>(rc.bottom - rc.top));
    g.inputBitmap = CreateBitmapFromRGBA(*g.inputPreview, w, h);
    SendMessage(g.imgInput, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g.inputBitmap);
}

static void UpdateModelPreviewBitmap() {
    ClearBitmap(g.modelBitmap);
    if (!g.previewMesh) {
        SendMessage(g.imgModel, STM_SETIMAGE, IMAGE_BITMAP, 0);
        return;
    }
    RECT rc{};
    GetClientRect(g.imgModel, &rc);
    int w = std::max<int>(10, static_cast<int>(rc.right - rc.left));
    int h = std::max<int>(10, static_cast<int>(rc.bottom - rc.top));
    g.modelBitmap = RenderMeshPreviewBitmap(*g.previewMesh, w, h, g.previewYaw);
    SendMessage(g.imgModel, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g.modelBitmap);
}

static void SetStatusText(const std::string& text) {
    SetWindowTextW(g.lblStatus, Widen(text).c_str());
}

static void SetReadyText(const std::string& text) {
    SetWindowTextW(g.lblReady, Widen(text).c_str());
}

static std::vector<std::string> OpenMultiplePngFiles(const char* title) {
    std::vector<char> buffer(65536, '\0');
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;
    if (!GetOpenFileNameA(&ofn)) return {};
    std::vector<std::string> result;
    const char* ptr = buffer.data();
    std::string first = ptr;
    ptr += first.size() + 1;
    if (*ptr == '\0') {
        result.push_back(first);
        return result;
    }
    fs::path dir = first;
    while (*ptr) {
        std::string name = ptr;
        result.push_back((dir / name).string());
        ptr += name.size() + 1;
    }
    return result;
}

static std::optional<std::string> OpenSingleVideoFile() {
    char buffer[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = "Video Files (*.mp4;*.mov;*.avi;*.wmv)\0*.mp4;*.mov;*.avi;*.wmv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = sizeof(buffer);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "動画ファイルを選んでください";
    if (!GetOpenFileNameA(&ofn)) return std::nullopt;
    return std::string(buffer);
}

static std::string FileNameOnly(const std::string& path) {
    return fs::path(path).filename().u8string();
}

static bool StringContainsDepthHint(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s.find("depth") != std::string::npos || s.find("_d") != std::string::npos || s.find("z") != std::string::npos;
}

static void RefreshFileList() {
    SendMessage(g.listFiles, LB_RESETCONTENT, 0, 0);
    if (g.mode == WorkflowMode::ImageDepth) {
        for (const auto& f : g.colorFiles) {
            std::wstring line = L"通常画像: " + Widen(FileNameOnly(f));
            SendMessageW(g.listFiles, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
        for (const auto& f : g.depthFiles) {
            std::wstring line = L"深度画像: " + Widen(FileNameOnly(f));
            SendMessageW(g.listFiles, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
        if (g.depthFiles.empty()) {
            SendMessageW(g.listFiles, LB_ADDSTRING, 0, (LPARAM)L"深度画像: なし（単画像から自動生成）");
        }
    } else {
        if (g.videoFile) {
            std::wstring line = L"動画: " + Widen(FileNameOnly(*g.videoFile));
            SendMessageW(g.listFiles, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
    }
}

static void AutoClassifyDroppedFiles(HDROP drop) {
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::string> addedColor;
    std::vector<std::string> addedDepth;
    std::optional<std::string> video;
    for (UINT i = 0; i < count; ++i) {
        wchar_t pathW[MAX_PATH] = {};
        DragQueryFileW(drop, i, pathW, MAX_PATH);
        fs::path path = pathW;
        std::string ext = path.extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".png") {
            std::string file = path.u8string();
            if (StringContainsDepthHint(FileNameOnly(file))) addedDepth.push_back(file);
            else addedColor.push_back(file);
        } else if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".wmv") {
            video = path.u8string();
        }
    }
    DragFinish(drop);
    if (video) {
        g.mode = WorkflowMode::Video;
        g.videoFile = video;
        g.colorFiles.clear();
        g.depthFiles.clear();
        ImageRGBA frame;
        std::string reason;
        if (ExtractRepresentativeVideoFrame(*video, frame, reason)) g.inputPreview = frame;
    } else {
        g.mode = WorkflowMode::ImageDepth;
        g.colorFiles.insert(g.colorFiles.end(), addedColor.begin(), addedColor.end());
        g.depthFiles.insert(g.depthFiles.end(), addedDepth.begin(), addedDepth.end());
        if (!g.colorFiles.empty()) g.inputPreview = LoadRGBA(g.colorFiles.front());
    }
    UpdateUI();
}

static bool IsReady(std::string& reason) {
    if (g.building) { reason = "今は処理中です。"; return false; }
    if (g.mode == WorkflowMode::ImageDepth) {
        if (g.colorFiles.empty()) { reason = "通常画像を追加してください。"; return false; }
        if (!g.depthFiles.empty() && g.colorFiles.size() != g.depthFiles.size()) { reason = "深度画像を使う場合は、通常画像と深度画像の枚数を同じにしてください。"; return false; }
        reason = g.depthFiles.empty()
            ? "準備ができました。単画像から立体化できます。"
            : "準備ができました。通常画像と深度画像の組み合わせで立体化できます。";
        return true;
    }
    if (!g.videoFile) { reason = "動画ファイルを1本追加してください。"; return false; }
    reason = "準備ができました。動画から立体化できます。";
    return true;
}

static QualityPreset CurrentQualityFromUI() {
    int sel = (int)SendMessage(g.comboQuality, CB_GETCURSEL, 0, 0);
    if (sel <= 0) return QualityPreset::Easy;
    if (sel == 1) return QualityPreset::Recommended;
    return QualityPreset::Detailed;
}

static ReconstructionPreset CurrentReconstructionModeFromUI() {
    int sel = (int)SendMessage(g.comboReconstruction, CB_GETCURSEL, 0, 0);
    if (sel <= 0) return ReconstructionPreset::Auto;
    if (sel == 1) return ReconstructionPreset::Relief;
    if (sel == 2) return ReconstructionPreset::PrimitiveBox;
    return ReconstructionPreset::HumanoidProxy;
}

static BuildResult BuildFromImages(QualityPreset preset) {
    BuildResult result;
    std::vector<ImageRGBA> colors;
    std::vector<DepthImage> depths;
    for (const auto& f : g.colorFiles) {
        auto img = LoadRGBA(f);
        if (!img) { result.message = "通常画像を開けませんでした: " + FileNameOnly(f); return result; }
        colors.push_back(*img);
    }
    for (const auto& f : g.depthFiles) {
        auto dep = LoadDepth(f);
        if (!dep) { result.message = "深度画像を開けませんでした: " + FileNameOnly(f); return result; }
        depths.push_back(*dep);
    }
    std::string reason;
    if (!depths.empty() && !ValidatePairDimensions(colors, depths, reason)) { result.message = reason; return result; }

    auto mask = BuildForegroundMaskForColors(colors);
    if (CountMaskPixels(mask) == 0) {
        result.message = "前景を抽出できませんでした。背景が単純なPNGか、アルファ付きPNGを試してください。";
        return result;
    }

    std::vector<float> fused;
    if (!depths.empty()) fused = FuseDepthMedianWeighted(depths);
    else fused = BuildPseudoDepthFromMask(mask, colors.front().width, colors.front().height);

    FillDepthHoles(fused, mask, colors.front().width, colors.front().height);
    int smoothPasses = (preset == QualityPreset::Easy) ? 1 : (preset == QualityPreset::Recommended ? 2 : 3);
    SmoothDepth(fused, mask, colors.front().width, colors.front().height, smoothPasses);

    float xyScale = 2.2f;
    float depthScale = (preset == QualityPreset::Easy) ? 0.9f : (preset == QualityPreset::Recommended ? 1.1f : 1.35f);
    float thickness = 0.12f;
    ReconstructionPreset rec = CurrentReconstructionModeFromUI();
    MeshData mesh = BuildSemanticMesh(fused, mask, colors.front().width, colors.front().height, xyScale, depthScale, thickness, rec);
    int lap = (rec == ReconstructionPreset::PrimitiveBox) ? 0 : ((preset == QualityPreset::Easy) ? 0 : (preset == QualityPreset::Recommended ? 1 : 2));
    RecomputeNormals(mesh);
    LaplacianSmooth(mesh, lap, 0.18f);

    fs::path out = ResolveOutputDir();
    fs::path texturePath = out / fs::path(g.colorFiles.front()).filename();
    std::error_code ec;
    fs::copy_file(g.colorFiles.front(), texturePath, fs::copy_options::overwrite_existing, ec);
    std::string stem = (rec == ReconstructionPreset::PrimitiveBox) ? "model_box" : (rec == ReconstructionPreset::HumanoidProxy ? "model_human" : (rec == ReconstructionPreset::Relief ? "model_relief" : "model_auto"));
    fs::path objPath = out / (stem + ".obj");
    if (!ExportOBJ(mesh, objPath, texturePath.filename().u8string())) {
        result.message = "OBJファイルの書き出しに失敗しました。";
        return result;
    }
    result.ok = true;
    result.message = depths.empty() ? "単画像から3Dモデルを書き出しました。" : "画像と深度画像から3Dモデルを書き出しました。";
    result.objPath = objPath;
    result.previewMesh = mesh;
    return result;
}

static BuildResult BuildFromVideo(QualityPreset preset) {
    BuildResult result;
    if (!g.videoFile) { result.message = "動画ファイルがありません。"; return result; }
    ImageRGBA frame;
    std::string reason;
    if (!ExtractRepresentativeVideoFrame(*g.videoFile, frame, reason)) { result.message = reason; return result; }
    auto mask = BuildMaskFromVideoFrame(frame);
    DepthImage depth = BuildDepthFromLuminance(frame, mask);
    std::vector<float> fused = depth.values;
    FillDepthHoles(fused, mask, frame.width, frame.height);
    int smoothPasses = (preset == QualityPreset::Easy) ? 1 : (preset == QualityPreset::Recommended ? 2 : 3);
    SmoothDepth(fused, mask, frame.width, frame.height, smoothPasses);
    ReconstructionPreset rec = CurrentReconstructionModeFromUI();
    if (rec == ReconstructionPreset::PrimitiveBox || rec == ReconstructionPreset::HumanoidProxy) {
        // 動画はまずフレームからの形状推定を優先し、必要なら auto に戻します。
        rec = ReconstructionPreset::Auto;
    }
    MeshData mesh = BuildSemanticMesh(fused, mask, frame.width, frame.height, 2.2f, 1.2f, 0.12f, rec);
    RecomputeNormals(mesh);
    LaplacianSmooth(mesh, (preset == QualityPreset::Detailed ? 2 : 1), 0.16f);

    fs::path out = ResolveOutputDir();
    fs::path tex = out / "video_frame.ppm";
    std::vector<unsigned char> rgb((size_t)frame.width * frame.height * 3, 0);
    for (int i = 0; i < frame.width * frame.height; ++i) {
        rgb[(size_t)i * 3 + 0] = frame.pixels[(size_t)i * 4 + 0];
        rgb[(size_t)i * 3 + 1] = frame.pixels[(size_t)i * 4 + 1];
        rgb[(size_t)i * 3 + 2] = frame.pixels[(size_t)i * 4 + 2];
    }
    SaveBinaryPPM(tex, frame.width, frame.height, rgb);
    fs::path objPath = out / "model_from_video.obj";
    if (!ExportOBJ(mesh, objPath, tex.filename().u8string())) {
        result.message = "動画から作ったモデルの書き出しに失敗しました。";
        return result;
    }
    result.ok = true;
    result.message = "動画から3Dモデルを書き出しました。";
    result.objPath = objPath;
    result.previewMesh = mesh;
    return result;
}

static void StartBuildAsync() {
    std::string reason;
    if (!IsReady(reason)) {
        ShowFriendlyError("まだ作れません", reason);
        return;
    }
    g.building = true;
    EnableWindow(g.btnBuild, FALSE);
    SetStatusText("処理中です… 入力に応じて3D形状を生成しています。");
    SendMessage(g.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(g.progress, PBM_SETPOS, 30, 0);
    QualityPreset preset = CurrentQualityFromUI();
    g.reconstruction = CurrentReconstructionModeFromUI();
    BuildResult result;
    try {
        result = (g.mode == WorkflowMode::ImageDepth) ? BuildFromImages(preset) : BuildFromVideo(preset);
    } catch (const std::exception& e) {
        result.ok = false;
        result.message = std::string("処理中に例外が発生しました: ") + e.what();
    } catch (...) {
        result.ok = false;
        result.message = "処理中に不明なエラーが発生しました。";
    }
    g_lastBuildResult = result;
    SendMessage(g.hwnd, WM_APP_BUILD_DONE, 0, 0);
}

static void ChooseOutputFolder() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = g.hwnd;
    bi.lpszTitle = L"出力先フォルダを選んでください";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, path)) {
        SetWindowTextW(g.editOutput, path);
    }
    CoTaskMemFree(pidl);
}

static void LoadSamples() {
    fs::path base = fs::current_path() / "samples";
    fs::path color = base / "input.png";
    fs::path depth = base / "depth.png";
    if (!fs::exists(color)) {
        ShowFriendlyError("サンプルが見つかりません", "samples フォルダ内に input.png が必要です。");
        return;
    }
    g.mode = WorkflowMode::ImageDepth;
    g.colorFiles = { color.u8string() };
    g.depthFiles.clear();
    if (fs::exists(depth)) g.depthFiles = { depth.u8string() };
    g.videoFile.reset();
    g.inputPreview = LoadRGBA(g.colorFiles.front());
    UpdateUI();
}

static void ResetAllInputs() {
    g.colorFiles.clear();
    g.depthFiles.clear();
    g.videoFile.reset();
    g.inputPreview.reset();
    g.previewMesh.reset();
    UpdateInputPreviewBitmap();
    UpdateModelPreviewBitmap();
    UpdateUI();
}

static void UpdateModeControls() {
    SendMessage(g.btnModeImage, BM_SETCHECK, g.mode == WorkflowMode::ImageDepth ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g.btnModeVideo, BM_SETCHECK, g.mode == WorkflowMode::Video ? BST_CHECKED : BST_UNCHECKED, 0);
    ShowWindow(g.btnAddColor, g.mode == WorkflowMode::ImageDepth ? SW_SHOW : SW_HIDE);
    ShowWindow(g.btnAddDepth, g.mode == WorkflowMode::ImageDepth ? SW_SHOW : SW_HIDE);
    ShowWindow(g.btnAddVideo, g.mode == WorkflowMode::Video ? SW_SHOW : SW_HIDE);
}

static void UpdateUI() {
    UpdateModeControls();
    SendMessage(g.comboReconstruction, CB_SETCURSEL, (WPARAM)((int)g.reconstruction), 0);
    SendMessage(g.comboQuality, CB_SETCURSEL, (WPARAM)((int)g.quality), 0);
    RefreshFileList();
    UpdateInputPreviewBitmap();
    UpdateModelPreviewBitmap();
    std::string reason;
    bool ready = IsReady(reason);
    SetReadyText(reason);
    EnableWindow(g.btnBuild, ready ? TRUE : FALSE);
    if (g.building) EnableWindow(g.btnBuild, FALSE);
}

static void DoQuickHelp() {
    ShowInfo(
        "使い方",
        "1. 上で『画像から作る』または『動画から作る』を選びます。\n"
        "2. ファイルを追加するか、『サンプルで試す』を押します。\n"
        "3. 形状タイプと品質を選びます。迷ったら『自動判定（おすすめ）』で大丈夫です。\n"
        "4. 『3Dモデルを作る』を押します。\n\n"
        "画像モードでは、通常画像だけでも立体化できます。深度画像は任意です。\n"
        "『箱 / 直方体』は四角シルエット向け、『人体プロキシ』は人の正面画像向けです。\n"
        "動画モードでは、1本の動画から代表フレームを使って立体化します。"
    );
}

static void LayoutControls(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    int pad = 12;
    int colLeft = 360;
    int colRight = W - colLeft - pad * 3;
    int y = pad;

    MoveWindow(g.lblTitle, pad, y, W - pad * 2, 28, TRUE); y += 28;
    MoveWindow(g.lblSubtitle, pad, y, W - pad * 2, 24, TRUE); y += 28;

    MoveWindow(g.grpWorkflow, pad, y, colLeft, 108, TRUE);
    MoveWindow(g.btnModeImage, pad + 16, y + 28, 150, 24, TRUE);
    MoveWindow(g.btnModeVideo, pad + 170, y + 28, 150, 24, TRUE);
    MoveWindow(g.btnUseSamples, pad + 16, y + 62, 140, 28, TRUE);
    MoveWindow(g.btnHelp, pad + 166, y + 62, 140, 28, TRUE);

    MoveWindow(g.grpPreview, colLeft + pad * 2, y, colRight, H - y - 110, TRUE);
    int previewY = y + 24;
    int previewW = (colRight - 36) / 2;
    int previewH = std::max(160, H - y - 170);
    MoveWindow(g.lblInputPreview, colLeft + pad * 2 + 14, previewY, previewW, 20, TRUE);
    MoveWindow(g.lblModelPreview, colLeft + pad * 2 + 22 + previewW, previewY, previewW, 20, TRUE);
    MoveWindow(g.imgInput, colLeft + pad * 2 + 14, previewY + 22, previewW, previewH, TRUE);
    MoveWindow(g.imgModel, colLeft + pad * 2 + 22 + previewW, previewY + 22, previewW, previewH, TRUE);

    y += 118;
    MoveWindow(g.grpInput, pad, y, colLeft, 220, TRUE);
    MoveWindow(g.btnAddColor, pad + 16, y + 28, 150, 28, TRUE);
    MoveWindow(g.btnAddDepth, pad + 174, y + 28, 150, 28, TRUE);
    MoveWindow(g.btnAddVideo, pad + 16, y + 28, 308, 28, TRUE);
    MoveWindow(g.btnReset, pad + 16, y + 62, 150, 28, TRUE);
    MoveWindow(g.listFiles, pad + 16, y + 98, colLeft - 32, 108, TRUE);

    y += 232;
    MoveWindow(g.grpOutput, pad, y, colLeft, 220, TRUE);
    MoveWindow(g.comboReconstruction, pad + 16, y + 30, colLeft - 32, 220, TRUE);
    MoveWindow(g.comboQuality, pad + 16, y + 68, 160, 180, TRUE);
    MoveWindow(g.editOutput, pad + 16, y + 106, colLeft - 32, 24, TRUE);
    MoveWindow(g.btnChooseOutput, pad + 16, y + 140, 150, 28, TRUE);
    MoveWindow(g.btnOpenOutput, pad + 174, y + 140, 150, 28, TRUE);
    MoveWindow(g.btnBuild, pad + 16, y + 174, colLeft - 32, 34, TRUE);

    MoveWindow(g.lblReady, pad, H - 90, W - pad * 2, 20, TRUE);
    MoveWindow(g.progress, pad, H - 64, W - pad * 2, 18, TRUE);
    MoveWindow(g.lblStatus, pad, H - 38, W - pad * 2, 20, TRUE);

    UpdateInputPreviewBitmap();
    UpdateModelPreviewBitmap();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g.hwnd = hwnd;
        g.cfg = LoadPortableConfig();
        DragAcceptFiles(hwnd, TRUE);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(nullptr, IDI_APPLICATION));

        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        g.lblTitle = CreateWindowW(L"STATIC", L"画像や動画から、かんたんに3Dモデルを作成", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.lblSubtitle = CreateWindowW(L"STATIC", L"Step 1: 作り方を選ぶ → Step 2: 素材を入れる → Step 3: 仕上がりを選ぶ → Step 4: 3Dモデルを書き出す", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        g.grpWorkflow = CreateWindowW(L"BUTTON", L"1. 作り方を選ぶ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.btnModeImage = CreateWindowW(L"BUTTON", L"画像から作る", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1001, nullptr, nullptr);
        g.btnModeVideo = CreateWindowW(L"BUTTON", L"動画から作る", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1002, nullptr, nullptr);
        g.btnUseSamples = CreateWindowW(L"BUTTON", L"サンプルで試す", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1003, nullptr, nullptr);
        g.btnHelp = CreateWindowW(L"BUTTON", L"使い方を見る", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1004, nullptr, nullptr);

        g.grpInput = CreateWindowW(L"BUTTON", L"2. 素材を入れる", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.btnAddColor = CreateWindowW(L"BUTTON", L"通常画像を追加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1101, nullptr, nullptr);
        g.btnAddDepth = CreateWindowW(L"BUTTON", L"深度画像を追加（任意）", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1102, nullptr, nullptr);
        g.btnAddVideo = CreateWindowW(L"BUTTON", L"動画を追加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1103, nullptr, nullptr);
        g.btnReset = CreateWindowW(L"BUTTON", L"リセット", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1104, nullptr, nullptr);
        g.listFiles = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_BORDER | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)1105, nullptr, nullptr);

        g.grpOutput = CreateWindowW(L"BUTTON", L"3. 仕上がりを選んで書き出す", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.comboReconstruction = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)1200, nullptr, nullptr);
        SendMessageW(g.comboReconstruction, CB_ADDSTRING, 0, (LPARAM)L"自動判定（おすすめ）");
        SendMessageW(g.comboReconstruction, CB_ADDSTRING, 0, (LPARAM)L"レリーフ / 立体化");
        SendMessageW(g.comboReconstruction, CB_ADDSTRING, 0, (LPARAM)L"箱 / 直方体");
        SendMessageW(g.comboReconstruction, CB_ADDSTRING, 0, (LPARAM)L"人体プロキシ");
        SendMessage(g.comboReconstruction, CB_SETCURSEL, 0, 0);
        g.comboQuality = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)1201, nullptr, nullptr);
        SendMessageW(g.comboQuality, CB_ADDSTRING, 0, (LPARAM)L"かんたん");
        SendMessageW(g.comboQuality, CB_ADDSTRING, 0, (LPARAM)L"おすすめ");
        SendMessageW(g.comboQuality, CB_ADDSTRING, 0, (LPARAM)L"高精細");
        SendMessage(g.comboQuality, CB_SETCURSEL, 1, 0);
        g.editOutput = CreateWindowW(L"EDIT", Widen((fs::current_path() / g.cfg.outputFolder).u8string()).c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)1202, nullptr, nullptr);
        g.btnChooseOutput = CreateWindowW(L"BUTTON", L"出力先を選ぶ", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1203, nullptr, nullptr);
        g.btnBuild = CreateWindowW(L"BUTTON", L"3Dモデルを作る", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1204, nullptr, nullptr);
        g.btnOpenOutput = CreateWindowW(L"BUTTON", L"出力先を開く", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1205, nullptr, nullptr);

        g.grpPreview = CreateWindowW(L"BUTTON", L"4. プレビュー", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.lblInputPreview = CreateWindowW(L"STATIC", L"入力プレビュー", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.lblModelPreview = CreateWindowW(L"STATIC", L"3Dプレビュー", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.imgInput = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | SS_BITMAP | SS_CENTERIMAGE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.imgModel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | SS_BITMAP | SS_CENTERIMAGE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        g.lblReady = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g.progress = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)1301, nullptr, nullptr);
        g.lblStatus = CreateWindowW(L"STATIC", L"サンプルで試すか、ファイルを追加してください。", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        for (HWND h : { g.lblTitle, g.lblSubtitle, g.grpWorkflow, g.btnModeImage, g.btnModeVideo, g.btnUseSamples, g.btnHelp,
                         g.grpInput, g.btnAddColor, g.btnAddDepth, g.btnAddVideo, g.btnReset, g.listFiles,
                         g.grpOutput, g.comboReconstruction, g.comboQuality, g.editOutput, g.btnChooseOutput, g.btnBuild, g.btnOpenOutput,
                         g.grpPreview, g.lblInputPreview, g.lblModelPreview, g.imgInput, g.imgModel, g.lblReady, g.progress, g.lblStatus }) {
            SendMessage(h, WM_SETFONT, (WPARAM)font, TRUE);
        }

        SetTimer(hwnd, TIMER_PREVIEW, 33, nullptr);
        UpdateUI();
        return 0;
    }
    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;
    case WM_DROPFILES:
        AutoClassifyDroppedFiles((HDROP)wParam);
        return 0;
    case WM_TIMER:
        if (wParam == TIMER_PREVIEW && g.previewMesh) {
            g.previewYaw += 0.03f;
            UpdateModelPreviewBitmap();
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        switch (id) {
        case 1001: g.mode = WorkflowMode::ImageDepth; g.videoFile.reset(); UpdateUI(); break;
        case 1002: g.mode = WorkflowMode::Video; g.colorFiles.clear(); g.depthFiles.clear(); UpdateUI(); break;
        case 1003: LoadSamples(); break;
        case 1004: DoQuickHelp(); break;
        case 1101: {
            auto files = OpenMultiplePngFiles("通常画像を選んでください");
            if (!files.empty()) {
                g.mode = WorkflowMode::ImageDepth;
                g.videoFile.reset();
                g.colorFiles.insert(g.colorFiles.end(), files.begin(), files.end());
                g.inputPreview = LoadRGBA(g.colorFiles.front());
                UpdateUI();
            }
            break;
        }
        case 1102: {
            auto files = OpenMultiplePngFiles("深度画像を選んでください");
            if (!files.empty()) {
                g.mode = WorkflowMode::ImageDepth;
                g.videoFile.reset();
                g.depthFiles.insert(g.depthFiles.end(), files.begin(), files.end());
                UpdateUI();
            }
            break;
        }
        case 1103: {
            auto file = OpenSingleVideoFile();
            if (file) {
                g.mode = WorkflowMode::Video;
                g.colorFiles.clear();
                g.depthFiles.clear();
                g.videoFile = file;
                ImageRGBA frame;
                std::string reason;
                if (ExtractRepresentativeVideoFrame(*file, frame, reason)) {
                    g.inputPreview = frame;
                } else {
                    g.inputPreview.reset();
                    SetStatusText(reason);
                }
                UpdateUI();
            }
            break;
        }
        case 1104: ResetAllInputs(); break;
        case 1200:
            if (code == CBN_SELCHANGE) {
                g.reconstruction = CurrentReconstructionModeFromUI();
                UpdateUI();
            }
            break;
        case 1201:
            if (code == CBN_SELCHANGE) {
                g.quality = CurrentQualityFromUI();
                UpdateUI();
            }
            break;
        case 1203: ChooseOutputFolder(); break;
        case 1204: StartBuildAsync(); break;
        case 1205: {
            fs::path out = ResolveOutputDir();
            ShellExecuteW(hwnd, L"open", out.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        }
        return 0;
    }
    case WM_APP_BUILD_DONE: {
        g.building = false;
        SendMessage(g.progress, PBM_SETPOS, 100, 0);
        if (g_lastBuildResult) {
            if (g_lastBuildResult->ok) {
                g.previewMesh = g_lastBuildResult->previewMesh;
                UpdateModelPreviewBitmap();
                SetStatusText(g_lastBuildResult->message + " 出力先フォルダを開けます。");
                ShowInfo("完了", g_lastBuildResult->message);
                SendMessage(g.progress, PBM_SETPOS, 0, 0);
            } else {
                SetStatusText(g_lastBuildResult->message);
                ShowFriendlyError("作成できませんでした", g_lastBuildResult->message);
                SendMessage(g.progress, PBM_SETPOS, 0, 0);
            }
        }
        UpdateUI();
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_PREVIEW);
        ClearBitmap(g.inputBitmap);
        ClearBitmap(g.modelBitmap);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInst;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"Make3DPortableWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Make3D Portable - かんたん3Dモデル作成", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 860, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

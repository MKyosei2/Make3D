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
    if (g.building) { reason = "今は処理中です。完了まで少し待ってください。"; return false; }
    if (g.mode == WorkflowMode::ImageDepth) {
        if (g.colorFiles.empty()) { reason = "通常画像を追加してください。"; return false; }
        if (g.depthFiles.empty()) { reason = "深度画像を追加してください。"; return false; }
        if (g.colorFiles.size() != g.depthFiles.size()) { reason = "通常画像と深度画像の枚数を同じにしてください。"; return false; }
    } else {
        if (!g.videoFile) { reason = "動画ファイルを1本追加してください。"; return false; }
    }
    reason = "準備ができました。『3Dモデルを作る』を押してください。";
    return true;
}

static QualityPreset CurrentQualityFromUI() {
    int sel = (int)SendMessage(g.comboQuality, CB_GETCURSEL, 0, 0);
    if (sel <= 0) return QualityPreset::Easy;
    if (sel == 1) return QualityPreset::Recommended;
    return QualityPreset::Detailed;
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
    if (!ValidatePairDimensions(colors, depths, reason)) { result.message = reason; return result; }

    auto mask = BuildAlphaMaskUnion(colors);
    auto fused = FuseDepthMedianWeighted(depths);
    FillDepthHoles(fused, mask, colors.front().width, colors.front().height);
    int smoothPasses = (preset == QualityPreset::Easy) ? 1 : (preset == QualityPreset::Recommended ? 2 : 3);
    SmoothDepth(fused, mask, colors.front().width, colors.front().height, smoothPasses);

    float xyScale = 2.2f;
    float depthScale = (preset == QualityPreset::Easy) ? 0.9f : (preset == QualityPreset::Recommended ? 1.1f : 1.35f);
    float thickness = 0.12f;
    MeshData mesh = BuildClosedDepthMesh(fused, mask, colors.front().width, colors.front().height, xyScale, depthScale, thickness);
    int lap = (preset == QualityPreset::Easy) ? 0 : (preset == QualityPreset::Recommended ? 1 : 2);
    RecomputeNormals(mesh);
    LaplacianSmooth(mesh, lap, 0.18f);

    fs::path out = ResolveOutputDir();
    fs::path texturePath = out / fs::path(g.colorFiles.front()).filename();
    std::error_code ec;
    fs::copy_file(g.colorFiles.front(), texturePath, fs::copy_options::overwrite_existing, ec);
    fs::path objPath = out / "model.obj";
    if (!ExportOBJ(mesh, objPath, texturePath.filename().u8string())) {
        result.message = "OBJファイルの書き出しに失敗しました。";
        return result;
    }
    result.ok = true;
    result.message = "3Dモデルを書き出しました。";
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
    MeshData mesh = BuildClosedDepthMesh(fused, mask, frame.width, frame.height, 2.2f, 1.2f, 0.12f);
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
    SetStatusText("処理中です… 完了までそのままお待ちください。");
    SendMessage(g.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(g.progress, PBM_SETPOS, 30, 0);
    QualityPreset preset = CurrentQualityFromUI();
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
    if (!fs::exists(color) || !fs::exists(depth)) {
        ShowFriendlyError("サンプルが見つかりません", "samples フォルダ内に input.png と depth.png が必要です。");
        return;
    }
    g.mode = WorkflowMode::ImageDepth;
    g.colorFiles = { color.u8string() };
    g.depthFiles = { depth.u8string() };
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
        "3. 仕上がりを選びます。迷ったら『おすすめ』のままで大丈夫です。\n"
        "4. 『3Dモデルを作る』を押します。\n\n"
        "画像モードでは、通常画像と深度画像の枚数を同じにしてください。\n"
        "動画モードでは、1本の動画だけで作れます。"
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
    MoveWindow(g.grpOutput, pad, y, colLeft, 188, TRUE);
    MoveWindow(g.comboQuality, pad + 16, y + 30, 160, 180, TRUE);
    MoveWindow(g.editOutput, pad + 16, y + 68, colLeft - 130, 24, TRUE);
    MoveWindow(g.btnChooseOutput, pad + colLeft - 106, y + 66, 90, 28, TRUE);
    MoveWindow(g.btnBuild, pad + 16, y + 108, 150, 34, TRUE);
    MoveWindow(g.btnOpenOutput, pad + 174, y + 108, 150, 34, TRUE);

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
        g.btnAddDepth = CreateWindowW(L"BUTTON", L"深度画像を追加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1102, nullptr, nullptr);
        g.btnAddVideo = CreateWindowW(L"BUTTON", L"動画を追加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1103, nullptr, nullptr);
        g.btnReset = CreateWindowW(L"BUTTON", L"リセット", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)1104, nullptr, nullptr);
        g.listFiles = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_BORDER | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)1105, nullptr, nullptr);

        g.grpOutput = CreateWindowW(L"BUTTON", L"3. 仕上がりを選んで書き出す", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
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
                         g.grpOutput, g.comboQuality, g.editOutput, g.btnChooseOutput, g.btnBuild, g.btnOpenOutput,
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

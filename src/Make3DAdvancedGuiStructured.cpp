#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include "Make3DStructuredAssetBuilder.h"
#include "Make3DGltfMaterialExporter.h"
#include "Make3DVertexColorGltfExporter.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int ID_COLOR = 1001;
constexpr int ID_DEPTH = 1002;
constexpr int ID_OUTPUT = 1003;
constexpr int ID_PREVIEW = 1004;
constexpr int ID_SAVE = 1005;
constexpr int ID_OPEN = 1006;
constexpr int ID_SAMPLE = 1007;
constexpr int ID_COLOR_PATH = 1010;
constexpr int ID_DEPTH_PATH = 1011;
constexpr int ID_OUTPUT_PATH = 1012;
constexpr int ID_STATUS = 1013;

LONG MaxLong(LONG a, LONG b) { return a > b ? a : b; }
LONG MinLong(LONG a, LONG b) { return a < b ? a : b; }
float MaxFloat(float a, float b) { return a > b ? a : b; }
float ClampFloat(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct GuiState {
    HWND hwnd = nullptr;
    HWND colorPath = nullptr;
    HWND depthPath = nullptr;
    HWND outputPath = nullptr;
    HWND status = nullptr;
    HWND previewButton = nullptr;
    HWND saveButton = nullptr;
    HWND openButton = nullptr;
    make3d::ImageRGBA image;
    make3d::MeshData mesh;
    make3d::StructuredAssetBuildResult structured;
    std::optional<fs::path> sourcePath;
    std::optional<fs::path> lastOutput;
    bool hasPreview = false;
};

GuiState g;

HMENU ControlId(int id) { return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)); }

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

std::string SafePathString(const fs::path& p) {
    try { return p.u8string(); } catch (...) { return "<path>"; }
}

void SetStatus(const std::string& text) {
    if (g.status) SetWindowTextW(g.status, Widen(text).c_str());
}

std::wstring GetText(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::vector<wchar_t> buffer(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, buffer.data(), len + 1);
    return std::wstring(buffer.data());
}

fs::path ExePath() {
    std::vector<wchar_t> buffer(32768, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len > 0 && len < static_cast<DWORD>(buffer.size())) return fs::path(buffer.data());
    return fs::path();
}

fs::path DefaultOutputDir() {
    fs::path exe = ExePath();
    if (!exe.empty()) return exe.parent_path() / L"advanced_output";
    return fs::path(L"advanced_output");
}

fs::path FindBundledSamplePng() {
    std::error_code ec;
    fs::path exe = ExePath();
    std::vector<fs::path> candidates;
    if (!exe.empty()) {
        fs::path dir = exe.parent_path();
        candidates.push_back(dir / L"samples" / L"input" / L"sample_character.png");
        candidates.push_back(dir.parent_path() / L"samples" / L"input" / L"sample_character.png");
        candidates.push_back(dir.parent_path().parent_path() / L"samples" / L"input" / L"sample_character.png");
        candidates.push_back(dir.parent_path().parent_path().parent_path() / L"samples" / L"input" / L"sample_character.png");
    }
    candidates.push_back(fs::current_path(ec) / L"samples" / L"input" / L"sample_character.png");
    for (const auto& c : candidates) if (!c.empty() && fs::exists(c, ec)) return c;
    return fs::path(L"samples") / L"input" / L"sample_character.png";
}

std::optional<fs::path> OpenImageDialog(HWND owner, const wchar_t* title) {
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.lpstrFilter = L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return fs::path(buffer.data());
}

std::optional<fs::path> BrowseFolder(HWND owner) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = L"Choose output folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return std::nullopt;
    std::vector<wchar_t> path(32768, L'\0');
    bool ok = SHGetPathFromIDListW(pidl, path.data()) != FALSE;
    CoTaskMemFree(pidl);
    if (!ok) return std::nullopt;
    return fs::path(path.data());
}

HWND Label(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

HWND Edit(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, w, h, parent, ControlId(id), nullptr, nullptr);
}

HWND Button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, parent, ControlId(id), nullptr, nullptr);
}

void EnableUi(bool enable) {
    EnableWindow(g.previewButton, enable ? TRUE : FALSE);
    EnableWindow(g.saveButton, enable && g.hasPreview ? TRUE : FALSE);
    EnableWindow(g.openButton, enable ? TRUE : FALSE);
}

void ClearPreview() {
    g.hasPreview = false;
    g.mesh = make3d::MeshData{};
    g.structured = make3d::StructuredAssetBuildResult{};
    InvalidateRect(g.hwnd, nullptr, TRUE);
    EnableUi(true);
}

bool BuildPreview(std::string& error) {
    std::wstring colorText = GetText(g.colorPath);
    if (colorText.empty()) { error = "Choose an input image first."; return false; }
    fs::path colorPath(colorText);
    auto color = make3d::LoadImageRGBA(colorPath, &error);
    if (!color) return false;

    std::optional<make3d::DepthImage> providedDepth;
    std::wstring depthText = GetText(g.depthPath);
    if (!depthText.empty()) providedDepth = make3d::LoadDepthImage(fs::path(depthText), &error);

    make3d::ReconstructionReport maskReport;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(*color, &maskReport);
    if (maskReport.foregroundPixels == 0) { error = "Foreground extraction failed. Use a clear object silhouette or PNG alpha."; return false; }

    make3d::AdvancedOptions adv;
    make3d::DepthImage depth = make3d::PrepareDepth(*color, providedDepth, mask, adv, &maskReport);
    make3d::StructuredAssetOptions options;
    options.targetHeight = 2.0f;
    options.defaultDepth = 0.70f;
    options.radialSegments = 40;
    options.addDetailParts = true;
    g.structured = make3d::BuildImageFittedStructuredAssetMesh(*color, depth, mask, options);
    if (!g.structured.ok) { error = g.structured.message; return false; }

    g.image = *color;
    g.mesh = g.structured.mesh;
    g.sourcePath = colorPath;
    g.hasPreview = true;
    return true;
}

POINT Project(float x, float y, float z, const RECT& r, float scale, float cx, float cy) {
    (void)r;
    float yaw = -0.68f, pitch = 0.34f;
    float cyaw = std::cos(yaw), syaw = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float rx = x * cyaw - z * syaw;
    float rz = x * syaw + z * cyaw;
    float ry = y * cp - rz * sp;
    return {static_cast<LONG>(cx + rx * scale), static_cast<LONG>(cy - ry * scale)};
}

float MeshUv(std::uint32_t vertex, int component) {
    const size_t idx = static_cast<size_t>(vertex) * 2 + static_cast<size_t>(component);
    if (idx < g.mesh.uvs.size()) return ClampFloat(g.mesh.uvs[idx], 0.0f, 1.0f);
    return 0.5f;
}

COLORREF SampleSourceColor(float u, float v) {
    if (g.image.width <= 0 || g.image.height <= 0 || g.image.pixels.size() < static_cast<size_t>(g.image.width * g.image.height * 4)) {
        return RGB(160, 170, 180);
    }
    u = ClampFloat(u, 0.0f, 1.0f);
    v = ClampFloat(v, 0.0f, 1.0f);
    const int x = ClampInt(static_cast<int>(u * static_cast<float>(g.image.width - 1) + 0.5f), 0, g.image.width - 1);
    const int y = ClampInt(static_cast<int>((1.0f - v) * static_cast<float>(g.image.height - 1) + 0.5f), 0, g.image.height - 1);
    const size_t p = (static_cast<size_t>(y) * static_cast<size_t>(g.image.width) + static_cast<size_t>(x)) * 4;
    return RGB(g.image.pixels[p + 0], g.image.pixels[p + 1], g.image.pixels[p + 2]);
}

COLORREF TrianglePreviewColor(std::uint32_t ia, std::uint32_t ib, std::uint32_t ic) {
    const COLORREF ca = SampleSourceColor(MeshUv(ia, 0), MeshUv(ia, 1));
    const COLORREF cb = SampleSourceColor(MeshUv(ib, 0), MeshUv(ib, 1));
    const COLORREF cc = SampleSourceColor(MeshUv(ic, 0), MeshUv(ic, 1));
    int r = (GetRValue(ca) + GetRValue(cb) + GetRValue(cc)) / 3;
    int gch = (GetGValue(ca) + GetGValue(cb) + GetGValue(cc)) / 3;
    int b = (GetBValue(ca) + GetBValue(cb) + GetBValue(cc)) / 3;
    return RGB(r, gch, b);
}

void DrawPreview(HDC hdc, HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    RECT r{16, 300, MaxLong(180L, rc.right - 16L), MaxLong(420L, rc.bottom - 16L)};
    HBRUSH bg = CreateSolidBrush(RGB(250, 250, 250));
    FillRect(hdc, &r, bg);
    DeleteObject(bg);
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SetBkMode(hdc, TRANSPARENT);
    RECT title = r; title.left += 10; title.top += 8;
    if (!g.hasPreview) {
        DrawTextW(hdc, L"High-quality fitted preview: choose an image, then press Preview 3D.", -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE);
        return;
    }
    DrawTextW(hdc, L"Color Image-Fitted 3D Preview - procedural parts + hybrid silhouette shell + source-color sampling", -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE);

    std::vector<POINT> pts(g.mesh.positions.size() / 3);
    float cx = (r.left + r.right) * 0.5f;
    float cy = (r.top + r.bottom) * 0.66f;
    LONG previewMin = MinLong(r.right - r.left, r.bottom - r.top);
    float scale = MaxFloat(90.0f, static_cast<float>(previewMin) * 0.34f);
    for (size_t i = 0; i < pts.size(); ++i) pts[i] = Project(g.mesh.positions[i * 3], g.mesh.positions[i * 3 + 1], g.mesh.positions[i * 3 + 2], r, scale, cx, cy);

    HPEN pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    for (size_t i = 0; i + 2 < g.mesh.indices.size(); i += 3) {
        std::uint32_t ia = g.mesh.indices[i], ib = g.mesh.indices[i + 1], ic = g.mesh.indices[i + 2];
        if (ia >= pts.size() || ib >= pts.size() || ic >= pts.size()) continue;
        HBRUSH brush = CreateSolidBrush(TrianglePreviewColor(ia, ib, ic));
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        POINT poly[3] = {pts[ia], pts[ib], pts[ic]};
        Polygon(hdc, poly, 3);
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }
    SelectObject(hdc, oldPen); DeleteObject(pen);

    RECT footer = r; footer.left += 10; footer.top = r.bottom - 30;
    std::ostringstream info;
    info << "type=" << make3d::ToString(g.structured.plan.assetType)
         << "  vertices=" << (g.mesh.positions.size() / 3)
         << "  triangles=" << (g.mesh.indices.size() / 3)
         << "  parts=" << g.structured.plan.parts.size()
         << "  path=image-fitted+color";
    DrawTextW(hdc, Widen(info.str()).c_str(), -1, &footer, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

void DoPreview() {
    try {
        EnableUi(false);
        SetStatus("Generating image-fitted structured preview...");
        std::string error;
        if (!BuildPreview(error)) {
            g.hasPreview = false;
            EnableUi(true);
            SetStatus("Preview failed: " + error);
            MessageBoxW(g.hwnd, Widen(error).c_str(), L"Preview failed", MB_OK | MB_ICONERROR);
            InvalidateRect(g.hwnd, nullptr, TRUE);
            return;
        }
        std::ostringstream oss;
        oss << "Preview ready. type=" << make3d::ToString(g.structured.plan.assetType)
            << ", vertices=" << (g.mesh.positions.size() / 3)
            << ", triangles=" << (g.mesh.indices.size() / 3)
            << ", parts=" << g.structured.plan.parts.size()
            << ". Path=image-fitted color preview.";
        SetStatus(oss.str());
        EnableUi(true);
        InvalidateRect(g.hwnd, nullptr, TRUE);
    } catch (const std::exception& e) {
        EnableUi(true);
        MessageBoxW(g.hwnd, Widen(e.what()).c_str(), L"Preview error", MB_OK | MB_ICONERROR);
    }
}

void DoSave() {
    if (!g.hasPreview || g.mesh.positions.empty()) {
        MessageBoxW(g.hwnd, L"Generate a preview first.", L"Make3D Advanced", MB_OK | MB_ICONWARNING);
        return;
    }
    try {
        std::wstring outputText = GetText(g.outputPath);
        fs::path output = outputText.empty() ? DefaultOutputDir() : fs::path(outputText);
        SetWindowTextW(g.outputPath, output.wstring().c_str());
        std::error_code ec;
        fs::create_directories(output, ec);
        fs::path objPath = output / L"make3d_structured_asset.obj";
        fs::path gltfPath = output / L"make3d_structured_asset.gltf";
        fs::path vertexColorGltfPath = output / L"make3d_structured_asset_vertex_color.gltf";
        fs::path reportPath = output / L"make3d_structured_asset_report.md";
        fs::path reportJsonPath = output / L"make3d_structured_asset_report.json";
        fs::path texturePath = output / L"make3d_source_image.png";
        if (g.sourcePath) fs::copy_file(*g.sourcePath, texturePath, fs::copy_options::overwrite_existing, ec);

        std::string error;
        if (!make3d::ExportOBJ(g.mesh, objPath, fs::exists(texturePath, ec) ? "make3d_source_image.png" : "", &error)) throw std::runtime_error(error);
        make3d::GltfMaterialOptions material;
        material.materialName = "Make3DStructuredAssetMaterial";
        material.baseColorFactor = {0.72f, 0.74f, 0.76f, 1.0f};
        material.roughnessFactor = 0.82f;
        material.doubleSided = true;
        if (fs::exists(texturePath, ec)) material.textureUri = texturePath;
        if (!make3d::ExportGLTFWithMaterial(g.mesh, gltfPath, material, &error)) throw std::runtime_error(error);

        make3d::VertexColorGltfOptions vertexColorOptions;
        vertexColorOptions.materialName = "Make3DImageFittedVertexColorMaterial";
        vertexColorOptions.roughnessFactor = 0.78f;
        vertexColorOptions.doubleSided = true;
        if (!make3d::ExportGLTFWithVertexColors(g.mesh, g.image, vertexColorGltfPath, vertexColorOptions, &error)) throw std::runtime_error(error);

        std::ofstream md(reportPath, std::ios::binary); md << g.structured.ToMarkdown();
        std::ofstream js(reportJsonPath, std::ios::binary); js << g.structured.ToJson();
        g.lastOutput = output;
        SetStatus("Saved OBJ/glTF plus vertex-color glTF: " + SafePathString(vertexColorGltfPath));
        MessageBoxW(g.hwnd, Widen("Saved image-fitted structured model.\n\nOBJ: " + SafePathString(objPath) + "\nglTF: " + SafePathString(gltfPath) + "\nVertex-color glTF: " + SafePathString(vertexColorGltfPath) + "\nReport: " + SafePathString(reportPath)).c_str(), L"Save finished", MB_OK | MB_ICONINFORMATION);
    } catch (const std::exception& e) {
        MessageBoxW(g.hwnd, Widen(e.what()).c_str(), L"Save failed", MB_OK | MB_ICONERROR);
    }
}

void OpenOutput() {
    fs::path output = !GetText(g.outputPath).empty() ? fs::path(GetText(g.outputPath)) : (g.lastOutput ? *g.lastOutput : DefaultOutputDir());
    ShellExecuteW(g.hwnd, L"open", output.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g.hwnd = hwnd;
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        Label(hwnd, L"Color image", 16, 18, 112, 24);
        Label(hwnd, L"Depth image", 16, 60, 112, 24);
        Label(hwnd, L"Output folder", 16, 102, 112, 24);
        g.colorPath = Edit(hwnd, ID_COLOR_PATH, 136, 18, 500, 28);
        g.depthPath = Edit(hwnd, ID_DEPTH_PATH, 136, 60, 500, 28);
        g.outputPath = Edit(hwnd, ID_OUTPUT_PATH, 136, 102, 500, 28);
        Button(hwnd, L"Choose color...", ID_COLOR, 650, 18, 130, 28);
        Button(hwnd, L"Use sample PNG", ID_SAMPLE, 790, 18, 130, 28);
        Button(hwnd, L"Choose depth...", ID_DEPTH, 650, 60, 130, 28);
        Button(hwnd, L"Choose output...", ID_OUTPUT, 650, 102, 130, 28);
        g.previewButton = Button(hwnd, L"Preview 3D", ID_PREVIEW, 136, 150, 160, 34);
        g.saveButton = Button(hwnd, L"Save OBJ/glTF", ID_SAVE, 310, 150, 160, 34);
        g.openButton = Button(hwnd, L"Open output", ID_OPEN, 484, 150, 150, 34);
        g.status = CreateWindowW(L"STATIC", L"Ready. High-quality path uses auto classification + silhouette-profile fitting.", WS_CHILD | WS_VISIBLE, 16, 205, 900, 64, hwnd, ControlId(ID_STATUS), nullptr, nullptr);
        SetWindowTextW(g.outputPath, DefaultOutputDir().wstring().c_str());
        HWND controls[] = {g.colorPath, g.depthPath, g.outputPath, g.previewButton, g.saveButton, g.openButton, g.status};
        for (HWND c : controls) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        EnableWindow(g.saveButton, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawPreview(hdc, hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_COLOR) {
            auto p = OpenImageDialog(hwnd, L"Choose color image");
            if (p) { SetWindowTextW(g.colorPath, p->wstring().c_str()); ClearPreview(); SetStatus("Selected color image: " + SafePathString(*p)); }
        } else if (id == ID_SAMPLE) {
            fs::path p = FindBundledSamplePng();
            SetWindowTextW(g.colorPath, p.wstring().c_str()); ClearPreview(); SetStatus("Selected sample: " + SafePathString(p));
        } else if (id == ID_DEPTH) {
            auto p = OpenImageDialog(hwnd, L"Choose optional depth image");
            if (p) { SetWindowTextW(g.depthPath, p->wstring().c_str()); ClearPreview(); SetStatus("Selected depth image: " + SafePathString(*p)); }
        } else if (id == ID_OUTPUT) {
            auto p = BrowseFolder(hwnd);
            if (p) { SetWindowTextW(g.outputPath, p->wstring().c_str()); SetStatus("Selected output: " + SafePathString(*p)); }
        } else if (id == ID_PREVIEW) DoPreview();
        else if (id == ID_SAVE) DoSave();
        else if (id == ID_OPEN) OpenOutput();
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    const wchar_t* kClassName = L"Make3DStructuredAdvancedGuiWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, kClassName, L"Make3D Advanced Engine - Image-Fitted Structured Builder", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 980, 740, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

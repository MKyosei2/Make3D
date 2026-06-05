#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include "Make3DGuiAdapter.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int ID_COLOR = 1001;
constexpr int ID_DEPTH = 1002;
constexpr int ID_OUTPUT = 1003;
constexpr int ID_PREVIEW = 1004;
constexpr int ID_OPEN = 1005;
constexpr int ID_MODE = 1006;
constexpr int ID_QUALITY = 1007;
constexpr int ID_STATUS = 1008;
constexpr int ID_COLOR_PATH = 1009;
constexpr int ID_DEPTH_PATH = 1010;
constexpr int ID_OUTPUT_PATH = 1011;
constexpr int ID_SAVE = 1012;

struct GuiState {
    HWND hwnd = nullptr;
    HWND colorPath = nullptr;
    HWND depthPath = nullptr;
    HWND outputPath = nullptr;
    HWND modeCombo = nullptr;
    HWND qualityCombo = nullptr;
    HWND previewButton = nullptr;
    HWND saveButton = nullptr;
    HWND openButton = nullptr;
    HWND status = nullptr;
    std::optional<fs::path> lastOutput;
    make3d::MeshData previewMesh;
    make3d::ReconstructionReport previewReport;
    bool hasPreview = false;
};

GuiState g;

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

std::string Narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring GetWindowTextWide(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::vector<wchar_t> buffer(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, buffer.data(), len + 1);
    return std::wstring(buffer.data());
}

void SetWindowTextUtf8(HWND hwnd, const std::string& text) {
    std::wstring wide = Widen(text);
    SetWindowTextW(hwnd, wide.c_str());
}

void SetStatus(const std::string& text) {
    SetWindowTextUtf8(g.status, text);
}

std::string SafePathString(const fs::path& p) {
    try { return p.u8string(); } catch (...) { return "<path>"; }
}

fs::path DefaultOutputDir() {
    wchar_t exePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        fs::path p(exePath);
        return p.parent_path() / L"advanced_output";
    }
    return fs::path(L"advanced_output");
}

std::optional<fs::path> OpenImageDialog(HWND owner, const wchar_t* title) {
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"PNG Images\0*.png\0Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return fs::path(buffer);
}

std::optional<fs::path> BrowseFolder(HWND owner) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = L"Choose output folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return std::nullopt;
    wchar_t path[MAX_PATH] = {};
    bool ok = SHGetPathFromIDListW(pidl, path) != FALSE;
    CoTaskMemFree(pidl);
    if (!ok) return std::nullopt;
    return fs::path(path);
}

void InitCombo(HWND combo, const std::vector<std::wstring>& items, int selected) {
    for (const auto& item : items) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

int ComboIndex(HWND combo, int fallback) {
    LRESULT result = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    return result == CB_ERR ? fallback : static_cast<int>(result);
}

void EnableUi(bool enable) {
    EnableWindow(g.previewButton, enable ? TRUE : FALSE);
    EnableWindow(g.saveButton, enable && g.hasPreview ? TRUE : FALSE);
    EnableWindow(g.openButton, enable ? TRUE : FALSE);
}

void ShowCaughtError(const char* title, const std::string& message) {
    SetStatus(std::string("Failed: ") + message);
    MessageBoxW(g.hwnd, Widen(message).c_str(), Widen(title).c_str(), MB_OK | MB_ICONERROR);
}

RECT PreviewRect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    RECT out{};
    out.left = 16;
    out.top = 356;
    out.right = std::max(out.left + 100, rc.right - 16);
    out.bottom = std::max(out.top + 100, rc.bottom - 16);
    return out;
}

make3d::AdvancedOptions CurrentOptions() {
    make3d::AdvancedOptions options = make3d::gui::OptionsFromGuiSelection(ComboIndex(g.modeCombo, 0), ComboIndex(g.qualityCombo, 1));
    options.exportObj = false;
    options.exportGltf = false;
    options.writeDebugImages = false;
    return options;
}

bool BuildPreviewMesh(std::string& error) {
    std::wstring colorText = GetWindowTextWide(g.colorPath);
    std::wstring depthText = GetWindowTextWide(g.depthPath);
    if (colorText.empty()) {
        error = "Choose a color PNG image first.";
        return false;
    }

    make3d::AdvancedOptions options = CurrentOptions();
    auto color = make3d::LoadImageRGBA(fs::path(colorText), &error);
    if (!color) return false;

    std::optional<make3d::DepthImage> providedDepth;
    if (!depthText.empty()) {
        providedDepth = make3d::LoadDepthImage(fs::path(depthText), &error);
    }

    g.previewReport = make3d::ReconstructionReport{};
    g.previewReport.imageWidth = color->width;
    g.previewReport.imageHeight = color->height;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(*color, &g.previewReport);
    if (g.previewReport.foregroundPixels == 0) {
        error = "Foreground extraction failed. Use a PNG with visible character/asset silhouette against a simple background.";
        return false;
    }

    make3d::DepthImage depth = make3d::PrepareDepth(*color, providedDepth, mask, options, &g.previewReport);
    g.previewMesh = make3d::ReconstructMesh(*color, depth, mask, options, &g.previewReport);
    if (g.previewMesh.positions.empty() || g.previewMesh.indices.empty()) {
        error = "Preview mesh generation failed.";
        return false;
    }
    make3d::RecomputeNormals(g.previewMesh);
    make3d::NormalizeMesh(g.previewMesh, 2.0f);
    g.previewReport.vertices = static_cast<int>(g.previewMesh.positions.size() / 3);
    g.previewReport.triangles = static_cast<int>(g.previewMesh.indices.size() / 3);
    g.previewReport.watertightCandidate = true;
    g.previewReport.reconstructionMode = "DetailedEditablePreview";
    g.hasPreview = true;
    return true;
}

void DoPreviewImpl() {
    EnableUi(false);
    SetStatus("Generating in-memory preview...");
    std::string error;
    if (!BuildPreviewMesh(error)) {
        g.hasPreview = false;
        EnableUi(true);
        SetStatus("Preview failed: " + error);
        MessageBoxW(g.hwnd, Widen(error).c_str(), L"Preview failed", MB_OK | MB_ICONERROR);
        InvalidateRect(g.hwnd, nullptr, TRUE);
        return;
    }

    std::ostringstream oss;
    oss << "Preview ready. vertices=" << g.previewReport.vertices
        << ", triangles=" << g.previewReport.triangles
        << ", mode=" << g.previewReport.reconstructionMode
        << ". Review the 3D view, then press Save OBJ/glTF.";
    SetStatus(oss.str());
    EnableUi(true);
    InvalidateRect(g.hwnd, nullptr, TRUE);
}

void DoPreview() {
    try {
        DoPreviewImpl();
    } catch (const std::system_error& e) {
        EnableUi(true);
        ShowCaughtError("System error", std::string("A filesystem or Windows path error occurred: ") + e.what());
    } catch (const std::exception& e) {
        EnableUi(true);
        ShowCaughtError("Preview error", std::string("An unexpected C++ error occurred: ") + e.what());
    } catch (...) {
        EnableUi(true);
        ShowCaughtError("Preview error", "An unknown error occurred while generating the preview.");
    }
}

void SavePreviewImpl() {
    if (!g.hasPreview || g.previewMesh.positions.empty()) {
        MessageBoxW(g.hwnd, L"Generate a preview first.", L"Make3D Advanced", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring outputText = GetWindowTextWide(g.outputPath);
    fs::path output = outputText.empty() ? DefaultOutputDir() : fs::path(outputText);
    SetWindowTextW(g.outputPath, output.wstring().c_str());

    std::error_code ec;
    fs::create_directories(output, ec);
    fs::path objPath = output / L"make3d_advanced.obj";
    fs::path gltfPath = output / L"make3d_advanced.gltf";
    fs::path reportPath = output / L"make3d_report.md";
    fs::path reportJsonPath = output / L"make3d_report.json";

    std::string error;
    if (!make3d::ExportOBJ(g.previewMesh, objPath, "", &error)) {
        ShowCaughtError("Save failed", error);
        return;
    }
    if (!make3d::ExportGLTF(g.previewMesh, gltfPath, &error)) {
        ShowCaughtError("Save failed", error);
        return;
    }

    g.previewReport.objPath = objPath;
    g.previewReport.gltfPath = gltfPath;
    g.previewReport.reportPath = reportPath;
    std::ofstream md(reportPath, std::ios::binary);
    md << g.previewReport.ToMarkdown();
    std::ofstream js(reportJsonPath, std::ios::binary);
    js << g.previewReport.ToJson();

    g.lastOutput = output;
    std::ostringstream oss;
    oss << "Saved OBJ/glTF after preview. OBJ=" << SafePathString(objPath)
        << ", glTF=" << SafePathString(gltfPath);
    SetStatus(oss.str());
    MessageBoxW(g.hwnd, Widen("Saved after preview.\n\nOBJ: " + SafePathString(objPath) + "\nglTF: " + SafePathString(gltfPath) + "\nReport: " + SafePathString(reportPath)).c_str(), L"Save finished", MB_OK | MB_ICONINFORMATION);
}

void DoSave() {
    try {
        SavePreviewImpl();
    } catch (const std::exception& e) {
        ShowCaughtError("Save failed", e.what());
    } catch (...) {
        ShowCaughtError("Save failed", "Unknown save error.");
    }
}

void OpenLastOutput() {
    try {
        std::wstring output = GetWindowTextWide(g.outputPath);
        fs::path path = !output.empty() ? fs::path(output) : (g.lastOutput ? *g.lastOutput : DefaultOutputDir());
        ShellExecuteW(g.hwnd, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } catch (const std::exception& e) {
        ShowCaughtError("Open output failed", e.what());
    }
}

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, parent, reinterpret_cast<HMENU>(id), nullptr, nullptr);
}

HWND CreateEdit(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, w, h, parent, reinterpret_cast<HMENU>(id), nullptr, nullptr);
}

void Layout(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int pad = 16;
    int labelW = 120;
    int rowH = 28;
    int editW = std::max(240, W - pad * 4 - labelW - 130);
    int y = 18;

    MoveWindow(g.colorPath, pad + labelW, y, editW, rowH, TRUE);
    y += 42;
    MoveWindow(g.depthPath, pad + labelW, y, editW, rowH, TRUE);
    y += 42;
    MoveWindow(g.outputPath, pad + labelW, y, editW, rowH, TRUE);
    y += 50;
    MoveWindow(g.modeCombo, pad + labelW, y, 220, rowH + 120, TRUE);
    y += 42;
    MoveWindow(g.qualityCombo, pad + labelW, y, 220, rowH + 90, TRUE);
    y += 52;
    MoveWindow(g.previewButton, pad + labelW, y, 160, 34, TRUE);
    MoveWindow(g.saveButton, pad + labelW + 174, y, 160, 34, TRUE);
    MoveWindow(g.openButton, pad + labelW + 348, y, 150, 34, TRUE);
    y += 52;
    MoveWindow(g.status, pad, y, W - pad * 2, 44, TRUE);
}

POINT ProjectVertex(float x, float y, float z, const RECT& r, float scale, float cx, float cy) {
    float yaw = -0.60f;
    float pitch = 0.28f;
    float cyaw = std::cos(yaw), syaw = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float rx = x * cyaw - z * syaw;
    float rz = x * syaw + z * cyaw;
    float ry = y * cp - rz * sp;
    LONG px = static_cast<LONG>(cx + rx * scale);
    LONG py = static_cast<LONG>(cy - ry * scale);
    px = std::clamp<LONG>(px, r.left - 2000, r.right + 2000);
    py = std::clamp<LONG>(py, r.top - 2000, r.bottom + 2000);
    return {px, py};
}

void DrawPreview(HDC hdc, HWND hwnd) {
    RECT r = PreviewRect(hwnd);
    HBRUSH bg = CreateSolidBrush(RGB(250, 250, 250));
    FillRect(hdc, &r, bg);
    DeleteObject(bg);
    HPEN border = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    HGDIOBJ oldPen = SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    RECT title = r;
    title.left += 10;
    title.top += 8;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(40, 40, 40));
    if (!g.hasPreview || g.previewMesh.positions.empty()) {
        DrawTextW(hdc, L"Preview area: press Preview 3D to generate an in-memory model before saving.", -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE);
        return;
    }

    std::wstring header = L"3D Preview - review this before saving";
    DrawTextW(hdc, header.c_str(), -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE);

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    std::vector<POINT> pts(g.previewMesh.positions.size() / 3);
    float scale0 = 120.0f;
    float centerX = (r.left + r.right) * 0.5f;
    float centerY = (r.top + r.bottom) * 0.60f;
    for (size_t i = 0; i < pts.size(); ++i) {
        float x = g.previewMesh.positions[i * 3 + 0];
        float y = g.previewMesh.positions[i * 3 + 1];
        float z = g.previewMesh.positions[i * 3 + 2];
        POINT p = ProjectVertex(x, y, z, r, scale0, centerX, centerY);
        minX = std::min(minX, static_cast<float>(p.x)); minY = std::min(minY, static_cast<float>(p.y));
        maxX = std::max(maxX, static_cast<float>(p.x)); maxY = std::max(maxY, static_cast<float>(p.y));
    }
    float spanX = std::max(1.0f, maxX - minX);
    float spanY = std::max(1.0f, maxY - minY);
    float usableW = std::max(50.0f, static_cast<float>(r.right - r.left - 60));
    float usableH = std::max(50.0f, static_cast<float>(r.bottom - r.top - 80));
    float scale = scale0 * std::min(usableW / spanX, usableH / spanY);
    centerX = (r.left + r.right) * 0.5f;
    centerY = (r.top + r.bottom) * 0.62f;
    for (size_t i = 0; i < pts.size(); ++i) {
        pts[i] = ProjectVertex(g.previewMesh.positions[i * 3 + 0], g.previewMesh.positions[i * 3 + 1], g.previewMesh.positions[i * 3 + 2], r, scale, centerX, centerY);
    }

    HPEN meshPen = CreatePen(PS_SOLID, 1, RGB(65, 78, 92));
    oldPen = SelectObject(hdc, meshPen);
    for (size_t i = 0; i + 2 < g.previewMesh.indices.size(); i += 3) {
        std::uint32_t ia = g.previewMesh.indices[i + 0];
        std::uint32_t ib = g.previewMesh.indices[i + 1];
        std::uint32_t ic = g.previewMesh.indices[i + 2];
        if (ia >= pts.size() || ib >= pts.size() || ic >= pts.size()) continue;
        MoveToEx(hdc, pts[ia].x, pts[ia].y, nullptr);
        LineTo(hdc, pts[ib].x, pts[ib].y);
        LineTo(hdc, pts[ic].x, pts[ic].y);
        LineTo(hdc, pts[ia].x, pts[ia].y);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(meshPen);

    std::ostringstream info;
    info << "vertices=" << g.previewReport.vertices << "  triangles=" << g.previewReport.triangles << "  mode=" << g.previewReport.reconstructionMode;
    RECT footer = r;
    footer.left += 10;
    footer.top = r.bottom - 28;
    DrawTextW(hdc, Widen(info.str()).c_str(), -1, &footer, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g.hwnd = hwnd;
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        CreateLabel(hwnd, L"Color image", 16, 18, 112, 24);
        CreateLabel(hwnd, L"Depth image", 16, 60, 112, 24);
        CreateLabel(hwnd, L"Output folder", 16, 102, 112, 24);
        CreateLabel(hwnd, L"Mode", 16, 152, 112, 24);
        CreateLabel(hwnd, L"Quality", 16, 194, 112, 24);
        g.colorPath = CreateEdit(hwnd, ID_COLOR_PATH, 136, 18, 360, 28);
        g.depthPath = CreateEdit(hwnd, ID_DEPTH_PATH, 136, 60, 360, 28);
        g.outputPath = CreateEdit(hwnd, ID_OUTPUT_PATH, 136, 102, 360, 28);
        CreateButton(hwnd, L"Choose color...", ID_COLOR, 510, 18, 130, 28);
        CreateButton(hwnd, L"Choose depth...", ID_DEPTH, 510, 60, 130, 28);
        CreateButton(hwnd, L"Choose output...", ID_OUTPUT, 510, 102, 130, 28);
        g.modeCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 136, 152, 240, 160, hwnd, reinterpret_cast<HMENU>(ID_MODE), nullptr, nullptr);
        g.qualityCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 136, 194, 240, 120, hwnd, reinterpret_cast<HMENU>(ID_QUALITY), nullptr, nullptr);
        InitCombo(g.modeCombo, {L"Auto", L"Relief Surface", L"Silhouette Volume", L"Hybrid Volume"}, 3);
        InitCombo(g.qualityCombo, {L"Draft", L"Standard", L"Detailed"}, 2);
        g.previewButton = CreateButton(hwnd, L"Preview 3D", ID_PREVIEW, 136, 246, 160, 34);
        g.saveButton = CreateButton(hwnd, L"Save OBJ/glTF", ID_SAVE, 310, 246, 160, 34);
        g.openButton = CreateButton(hwnd, L"Open output", ID_OPEN, 484, 246, 150, 34);
        g.status = CreateWindowW(L"STATIC", L"Ready. Press Preview 3D first; files are saved only after Save OBJ/glTF.", WS_CHILD | WS_VISIBLE, 16, 300, 620, 44, hwnd, reinterpret_cast<HMENU>(ID_STATUS), nullptr, nullptr);
        SetWindowTextW(g.outputPath, DefaultOutputDir().wstring().c_str());
        HWND controls[] = { g.colorPath, g.depthPath, g.outputPath, g.modeCombo, g.qualityCombo, g.previewButton, g.saveButton, g.openButton, g.status };
        for (HWND c : controls) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        EnableWindow(g.saveButton, FALSE);
        Layout(hwnd);
        return 0;
    }
    case WM_SIZE:
        Layout(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawPreview(hdc, hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        try {
            if (id == ID_COLOR) {
                auto path = OpenImageDialog(hwnd, L"Choose color PNG image");
                if (path) { SetWindowTextW(g.colorPath, path->wstring().c_str()); g.hasPreview = false; EnableUi(true); InvalidateRect(hwnd, nullptr, TRUE); }
            } else if (id == ID_DEPTH) {
                auto path = OpenImageDialog(hwnd, L"Choose optional depth image");
                if (path) { SetWindowTextW(g.depthPath, path->wstring().c_str()); g.hasPreview = false; EnableUi(true); InvalidateRect(hwnd, nullptr, TRUE); }
            } else if (id == ID_OUTPUT) {
                auto path = BrowseFolder(hwnd);
                if (path) SetWindowTextW(g.outputPath, path->wstring().c_str());
            } else if (id == ID_PREVIEW) {
                DoPreview();
            } else if (id == ID_SAVE) {
                DoSave();
            } else if (id == ID_OPEN) {
                OpenLastOutput();
            }
        } catch (const std::exception& e) {
            ShowCaughtError("GUI command failed", e.what());
        } catch (...) {
            ShowCaughtError("GUI command failed", "Unknown GUI command error.");
        }
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

    const wchar_t* kClassName = L"Make3DAdvancedGuiWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Make3D Advanced Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        920,
        720,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

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

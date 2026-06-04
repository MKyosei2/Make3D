#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include "Make3DGuiAdapter.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int ID_COLOR = 1001;
constexpr int ID_DEPTH = 1002;
constexpr int ID_OUTPUT = 1003;
constexpr int ID_BUILD = 1004;
constexpr int ID_OPEN = 1005;
constexpr int ID_MODE = 1006;
constexpr int ID_QUALITY = 1007;
constexpr int ID_STATUS = 1008;
constexpr int ID_COLOR_PATH = 1009;
constexpr int ID_DEPTH_PATH = 1010;
constexpr int ID_OUTPUT_PATH = 1011;

struct GuiState {
    HWND hwnd = nullptr;
    HWND colorPath = nullptr;
    HWND depthPath = nullptr;
    HWND outputPath = nullptr;
    HWND modeCombo = nullptr;
    HWND qualityCombo = nullptr;
    HWND buildButton = nullptr;
    HWND openButton = nullptr;
    HWND status = nullptr;
    std::optional<fs::path> lastOutput;
};

GuiState g;

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

std::string Narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::string GetWindowTextUtf8(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring text(static_cast<size_t>(len), L'\0');
    GetWindowTextW(hwnd, text.data(), len + 1);
    return Narrow(text);
}

void SetWindowTextUtf8(HWND hwnd, const std::string& text) {
    std::wstring wide = Widen(text);
    SetWindowTextW(hwnd, wide.c_str());
}

void SetStatus(const std::string& text) {
    SetWindowTextUtf8(g.status, text);
}

std::optional<fs::path> OpenImageDialog(HWND owner, const wchar_t* title) {
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.psd;*.gif;*.hdr;*.pic;*.ppm;*.pgm\0All Files\0*.*\0";
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
    EnableWindow(g.buildButton, enable ? TRUE : FALSE);
}

void DoBuild() {
    std::string color = GetWindowTextUtf8(g.colorPath);
    std::string depth = GetWindowTextUtf8(g.depthPath);
    std::string output = GetWindowTextUtf8(g.outputPath);

    if (color.empty()) {
        MessageBoxW(g.hwnd, L"Choose a color image first.", L"Make3D Advanced", MB_OK | MB_ICONWARNING);
        return;
    }
    if (output.empty()) output = (fs::current_path() / "advanced_output").u8string();

    make3d::gui::GuiBuildRequest request;
    request.colorPath = fs::path(color);
    if (!depth.empty()) request.depthPath = fs::path(depth);
    request.outputDir = fs::path(output);
    request.guiReconstructionIndex = ComboIndex(g.modeCombo, 0);
    request.guiQualityIndex = ComboIndex(g.qualityCombo, 1);
    request.exportObj = true;
    request.exportGltf = true;
    request.writeDebugImages = true;

    EnableUi(false);
    SetStatus("Building advanced 3D model...");

    make3d::gui::GuiBuildSummary summary = make3d::gui::BuildAdvancedFromGuiRequest(request);

    EnableUi(true);
    if (!summary.ok) {
        SetStatus("Failed: " + summary.message);
        MessageBoxW(g.hwnd, Widen(summary.message).c_str(), L"Build failed", MB_OK | MB_ICONERROR);
        return;
    }

    g.lastOutput = request.outputDir;
    std::ostringstream oss;
    oss << summary.message << "\n\nOBJ: " << summary.objPath.u8string()
        << "\nglTF: " << summary.gltfPath.u8string()
        << "\nReport: " << summary.reportPath.u8string();
    SetStatus(summary.message);
    MessageBoxW(g.hwnd, Widen(oss.str()).c_str(), L"Build finished", MB_OK | MB_ICONINFORMATION);
}

void OpenLastOutput() {
    std::string output = GetWindowTextUtf8(g.outputPath);
    fs::path path = !output.empty() ? fs::path(output) : (g.lastOutput ? *g.lastOutput : fs::current_path() / "advanced_output");
    ShellExecuteW(g.hwnd, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
    int buttonW = 130;
    int rowH = 28;
    int editW = std::max(240, W - pad * 4 - labelW - buttonW);
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
    MoveWindow(g.buildButton, pad + labelW, y, 200, 34, TRUE);
    MoveWindow(g.openButton, pad + labelW + 214, y, 160, 34, TRUE);
    y += 52;
    MoveWindow(g.status, pad, y, W - pad * 2, 64, TRUE);
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
        InitCombo(g.qualityCombo, {L"Draft", L"Standard", L"Detailed"}, 1);
        g.buildButton = CreateButton(hwnd, L"Build Advanced 3D", ID_BUILD, 136, 246, 200, 34);
        g.openButton = CreateButton(hwnd, L"Open output", ID_OPEN, 350, 246, 160, 34);
        g.status = CreateWindowW(L"STATIC", L"Ready. Choose an image and build an advanced proxy model.", WS_CHILD | WS_VISIBLE, 16, 300, 620, 64, hwnd, reinterpret_cast<HMENU>(ID_STATUS), nullptr, nullptr);

        SetWindowTextUtf8(g.outputPath, (fs::current_path() / "advanced_output").u8string());
        HWND controls[] = { g.colorPath, g.depthPath, g.outputPath, g.modeCombo, g.qualityCombo, g.buildButton, g.openButton, g.status };
        for (HWND c : controls) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        Layout(hwnd);
        return 0;
    }
    case WM_SIZE:
        Layout(hwnd);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_COLOR) {
            auto path = OpenImageDialog(hwnd, L"Choose color image");
            if (path) SetWindowTextW(g.colorPath, path->wstring().c_str());
        } else if (id == ID_DEPTH) {
            auto path = OpenImageDialog(hwnd, L"Choose optional depth image");
            if (path) SetWindowTextW(g.depthPath, path->wstring().c_str());
        } else if (id == ID_OUTPUT) {
            auto path = BrowseFolder(hwnd);
            if (path) SetWindowTextW(g.outputPath, path->wstring().c_str());
        } else if (id == ID_BUILD) {
            DoBuild();
        } else if (id == ID_OPEN) {
            OpenLastOutput();
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
        720,
        430,
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

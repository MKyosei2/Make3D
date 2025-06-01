#include <windows.h>
#include <commdlg.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include "GUIState.h"
#include "PNGLoader.h"
#include "PartVolumeBuilder.h"
#include "FBXExporter.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

GUIState guiState;

void SaveProject(const char* path) {
    std::ofstream ofs(path);
    for (const auto& [part, views] : guiState.partViewImages) {
        for (const auto& [view, paths] : views) {
            for (const auto& p : paths) {
                ofs << (int)part << " " << (int)view << " " << p << "\n";
            }
        }
    }
}

void LoadProject(const char* path) {
    std::ifstream ifs(path);
    guiState.partViewImages.clear();
    int part, view;
    std::string line;
    while (std::getline(ifs, line)) {
        size_t p1 = line.find(' ');
        size_t p2 = line.find(' ', p1 + 1);
        part = std::stoi(line.substr(0, p1));
        view = std::stoi(line.substr(p1 + 1, p2 - p1 - 1));
        std::string file = line.substr(p2 + 1);
        guiState.partViewImages[(PartType)part][(ViewType)view].push_back(file);
    }
}

void ExportAllFBX(bool combine) {
    auto volumes = BuildVolumesFromImages(guiState.partViewImages);
    if (combine) {
        Mesh3D combined;
        for (auto& [part, vol] : volumes) {
            Mesh3D mesh = BuildMeshFromVolume(vol);
            AppendMesh(combined, mesh);
        }
        ExportToFBX(combined, "combined_output.fbx");
    }
    else {
        for (auto& [part, vol] : volumes) {
            Mesh3D mesh = BuildMeshFromVolume(vol);
            char filename[128];
            sprintf_s(filename, "part_%d.fbx", (int)part);
            ExportToFBX(mesh, filename);
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    HWND hwnd = CreateWindowEx(0, "STATIC", "3D Model Generator", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 640, 480, nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void AddImageFile() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.lpstrFilter = "PNG Files\0*.png\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        // 仮に Head/Front として追加（UIがないのでデフォルト）
        guiState.partViewImages[PartType::Head][ViewType::Front].push_back(filename);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindow("BUTTON", "画像追加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 20, 100, 30, hwnd, (HMENU)1, nullptr, nullptr);
        CreateWindow("BUTTON", "保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, 20, 100, 30, hwnd, (HMENU)2, nullptr, nullptr);
        CreateWindow("BUTTON", "読み込み", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            260, 20, 100, 30, hwnd, (HMENU)3, nullptr, nullptr);
        CreateWindow("BUTTON", "FBX出力", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 20, 100, 30, hwnd, (HMENU)4, nullptr, nullptr);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: AddImageFile(); break;
        case 2: SaveProject("project.txt"); break;
        case 3: LoadProject("project.txt"); break;
        case 4: ExportAllFBX(false); break;
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

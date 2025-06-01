#include <windows.h>
#include <commdlg.h>
#include "GUIState.h"
#include "PartTypes.h"
#include "PNGLoader.h"
#include "PartVolumeBuilder.h"
#include "VolumeMerger.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#include "ProjectIO.h"

GUIState guiState;

const char* partLabels[] = { "Head", "Body", "Arm", "Leg", "Other" };
const char* viewLabels[] = { "Front", "Back", "Left", "Right", "Top", "Bottom" };

void SelectImage(HWND hwnd) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.lpstrFilter = "PNG Files\0*.png\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.hwndOwner = hwnd;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        guiState.imagePaths[guiState.selectedPart][guiState.selectedView] = filename;
        MessageBoxA(hwnd, filename, "登録完了", MB_OK);
    }
}

void ExportFBX() {
    std::vector<Mesh3D> meshes;

    for (const auto& part : guiState.imagePaths) {
        std::map<ViewType, Image2D> imgs;
        for (const auto& view : part.second) {
            imgs[view.first] = LoadPNG(view.second.c_str());
        }
        Volume3D vol = BuildVolumeFromImages(imgs);
        Mesh3D mesh = GenerateMeshFromVolume(vol, guiState.polygonCount);
        meshes.push_back(mesh);
    }

    if (guiState.exportCombined) {
        Mesh3D combined;
        int offset = 0;
        for (const auto& m : meshes) {
            combined.vertices.insert(combined.vertices.end(), m.vertices.begin(), m.vertices.end());
            for (int idx : m.indices)
                combined.indices.push_back(idx + offset);
            offset += (int)m.vertices.size();
        }
        ExportToFBX(combined, "output.fbx");
    }
    else {
        int i = 0;
        for (const auto& m : meshes) {
            char name[64];
            sprintf_s(name, "part_%d.fbx", i++);
            ExportToFBX(m, name);
        }
    }
    MessageBoxA(NULL, "FBX出力完了", "完了", MB_OK);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("STATIC", "パーツ:", WS_VISIBLE | WS_CHILD, 20, 20, 60, 20, hwnd, NULL, NULL, NULL);
        CreateWindowA("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 80, 20, 100, 200, hwnd, (HMENU)100, NULL, NULL);
        for (int i = 0; i < 5; ++i)
            SendMessageA(GetDlgItem(hwnd, 100), CB_ADDSTRING, 0, (LPARAM)partLabels[i]);
        SendMessageA(GetDlgItem(hwnd, 100), CB_SETCURSEL, 0, 0);

        CreateWindowA("STATIC", "視点:", WS_VISIBLE | WS_CHILD, 200, 20, 60, 20, hwnd, NULL, NULL, NULL);
        CreateWindowA("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 260, 20, 100, 200, hwnd, (HMENU)101, NULL, NULL);
        for (int i = 0; i < 6; ++i)
            SendMessageA(GetDlgItem(hwnd, 101), CB_ADDSTRING, 0, (LPARAM)viewLabels[i]);
        SendMessageA(GetDlgItem(hwnd, 101), CB_SETCURSEL, 0, 0);

        CreateWindowA("BUTTON", "画像追加", WS_VISIBLE | WS_CHILD, 20, 60, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        CreateWindowA("BUTTON", "FBX出力", WS_VISIBLE | WS_CHILD, 140, 60, 100, 30, hwnd, (HMENU)2, NULL, NULL);

        CreateWindowA("STATIC", "ポリゴン数:", WS_VISIBLE | WS_CHILD, 20, 110, 80, 20, hwnd, NULL, NULL, NULL);
        CreateWindowA("EDIT", "1000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 100, 110, 60, 20, hwnd, (HMENU)3, NULL, NULL);

        CreateWindowA("BUTTON", "結合出力", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 200, 110, 100, 20, hwnd, (HMENU)4, NULL, NULL);
        SendMessageA(GetDlgItem(hwnd, 4), BM_SETCHECK, BST_CHECKED, 0);

        CreateWindowA("BUTTON", "保存", WS_VISIBLE | WS_CHILD, 20, 150, 80, 30, hwnd, (HMENU)5, NULL, NULL);
        CreateWindowA("BUTTON", "読込", WS_VISIBLE | WS_CHILD, 120, 150, 80, 30, hwnd, (HMENU)6, NULL, NULL);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            int selPart = SendMessageA(GetDlgItem(hwnd, 100), CB_GETCURSEL, 0, 0);
            int selView = SendMessageA(GetDlgItem(hwnd, 101), CB_GETCURSEL, 0, 0);
            guiState.selectedPart = (PartType)selPart;
            guiState.selectedView = (ViewType)selView;
        }
        switch (LOWORD(wParam)) {
        case 1: SelectImage(hwnd); break;
        case 2: {
            char buf[16];
            GetWindowTextA(GetDlgItem(hwnd, 3), buf, 16);
            guiState.polygonCount = atoi(buf);
            guiState.exportCombined = (SendMessageA(GetDlgItem(hwnd, 4), BM_GETCHECK, 0, 0) == BST_CHECKED);
            ExportFBX();
            break;
        }
        case 5:
            SaveProject("project.txt", guiState);
            MessageBoxA(hwnd, "保存完了", "完了", MB_OK);
            break;
        case 6:
            if (LoadProject("project.txt", guiState)) {
                MessageBoxA(hwnd, "読込完了", "完了", MB_OK);
            }
            else {
                MessageBoxA(hwnd, "読込失敗", "エラー", MB_OK | MB_ICONERROR);
            }
            break;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0, 0, hInstance,
        NULL, NULL, NULL, NULL, "3DGenWin", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowA("3DGenWin", "3D Generator", WS_OVERLAPPEDWINDOW,
        100, 100, 400, 250, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
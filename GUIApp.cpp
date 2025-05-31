#include "PNGLoader.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <string>

#pragma comment(lib, "d3d11.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static std::string selectedFile;
static int polygonLimit = 500;

void RenderUI() {
    ImGui::Begin("3D Generator");

    if (ImGui::Button("Select PNG")) {
        char filename[MAX_PATH] = "";
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "PNG Files\0*.png\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            selectedFile = filename;
        }
    }
    ImGui::Text("Selected: %s", selectedFile.c_str());
    ImGui::SliderInt("Polygon Limit", &polygonLimit, 100, 10000);

    if (ImGui::Button("Generate FBX")) {
        if (!selectedFile.empty()) {
            std::vector<unsigned char> image;
            int w = 0, h = 0;
            if (LoadPNG(selectedFile, image, w, h)) {
                auto mesh = GenerateMeshFromSilhouette(image, w, h);
                std::string outPath = selectedFile + ".fbx";
                ExportToFBX(mesh, outPath.c_str());
            }
        }
    }
    ImGui::End();
}

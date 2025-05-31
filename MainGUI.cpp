#include "GUIState.h"
#include "VolumeBuilder.h"
#include "VolumeToMesh.h"
#include "FBXExporter.h"
#include "PNGLoader.h"
#include "imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <tuple>
#include <vector>

extern GUIState guiState;

static const char* ViewNames[] = {
    "Front", "Back", "Left", "Right", "Top", "Bottom"
};

void ShowMainGUI() {
    ImGui::Begin("3D Model Generator");

    // パーツ名入力
    static char labelBuffer[64] = "part";
    if (ImGui::InputText("パーツ名", labelBuffer, sizeof(labelBuffer))) {
        guiState.currentPartName = std::string(labelBuffer);
    }

    // 視点選択
    int currentView = static_cast<int>(guiState.selectedView);
    if (ImGui::Combo("視点選択", &currentView, ViewNames, IM_ARRAYSIZE(ViewNames))) {
        guiState.selectedView = static_cast<ViewDirection>(currentView);
    }

    // 画像追加ボタン
    if (ImGui::Button("画像追加")) {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "PNG Files\0*.png\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            guiState.allParts[guiState.currentPartName][guiState.selectedView].push_back(filename);
        }
    }

    // 表示：登録済み画像
    for (const auto& [partName, views] : guiState.allParts) {
        ImGui::Text("Part: %s", partName.c_str());
        for (const auto& [view, paths] : views) {
            ImGui::Text("  [%s]", ViewNames[static_cast<int>(view)]);
            for (const auto& path : paths) {
                ImGui::BulletText("    %s", path.c_str());
            }
        }
    }

    // ポリゴン数指定
    ImGui::SliderInt("ポリゴン数", &guiState.polygonCount, 100, 10000);

    // 出力ボタン
    if (ImGui::Button("すべてのパーツをFBX出力")) {
        try {
            for (const auto& [partName, views] : guiState.allParts) {
                std::vector<std::tuple<std::string, std::string, Image2D>> labeledImages;
                for (const auto& [view, paths] : views) {
                    std::string viewStr = ViewNames[static_cast<int>(view)];
                    for (const auto& path : paths) {
                        Image2D img = LoadPNG(path.c_str());
                        labeledImages.emplace_back(partName, viewStr, img);
                    }
                }

                auto volumes = BuildVolumesFromLabeledImages(labeledImages);
                for (const auto& lv : volumes) {
                    auto mesh = ConvertVolumeToMesh(lv.volume, guiState.polygonCount);
                    std::string outPath = partName + ".fbx";
                    ExportToFBX(mesh, outPath.c_str());
                }
            }
        }
        catch (const std::exception& e) {
            MessageBoxA(nullptr, e.what(), "エラー", MB_OK);
        }
    }

    ImGui::End();
}

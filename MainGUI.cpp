#include "GUIState.h"
#include "PartVolumeBuilder.h"
#include "VolumeToMesh.h"
#include "FBXExporter.h"
#include "MeshUtils.h"
#include "imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <nlohmann/json.hpp>

GUIState guiState;

static const char* PartNames[] = { "Head", "Body", "Arm", "Leg", "Tail", "Wing", "Unknown" };
static const char* ViewNames[] = { "Front", "Back", "Left", "Right", "Top", "Bottom" };

void SaveProject(const std::string& filename) {
    nlohmann::json j;
    for (const auto& [part, views] : guiState.partViewImages) {
        std::string partStr = PartNames[(int)part];
        for (const auto& [view, paths] : views) {
            std::string viewStr = ViewNames[(int)view];
            j[partStr][viewStr] = paths;
        }
    }
    std::ofstream ofs(filename);
    ofs << j.dump(2);
}

void LoadProject(const std::string& filename) {
    std::ifstream ifs(filename);
    nlohmann::json j;
    ifs >> j;
    guiState.partViewImages.clear();
    for (auto& [partStr, views] : j.items()) {
        PartType part = PartType::Unknown;
        for (int i = 0; i < 7; ++i) if (partStr == PartNames[i]) part = (PartType)i;
        for (auto& [viewStr, paths] : views.items()) {
            ViewType view = ViewType::Front;
            for (int v = 0; v < 6; ++v) if (viewStr == ViewNames[v]) view = (ViewType)v;
            guiState.partViewImages[part][view] = paths.get<std::vector<std::string>>();
        }
    }
}

void ShowMainGUI() {
    ImGui::Begin("3D Model Generator");

    int currentPart = (int)guiState.selectedPart;
    if (ImGui::Combo("パーツ", &currentPart, PartNames, 7)) {
        guiState.selectedPart = (PartType)currentPart;
    }

    int currentView = (int)guiState.selectedView;
    if (ImGui::Combo("視点", &currentView, ViewNames, 6)) {
        guiState.selectedView = (ViewType)currentView;
    }

    ImGui::SliderInt("ポリゴン数", &guiState.polygonCount, 100, 10000);

    const char* scaleLabels[] = { "Unity (1.0m)", "Maya (0.01m)" };
    int scaleIdx = static_cast<int>(guiState.scaleUnit);
    if (ImGui::Combo("出力スケール", &scaleIdx, scaleLabels, 2)) {
        guiState.scaleUnit = (ExportScaleUnit)scaleIdx;
    }

    ImGui::Checkbox("FBXを結合出力", &guiState.exportCombined);

    if (ImGui::Button("画像追加")) {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "PNG Files\0*.png\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            guiState.partViewImages[guiState.selectedPart][guiState.selectedView].push_back(filename);
        }
    }

    for (auto& [view, paths] : guiState.partViewImages[guiState.selectedPart]) {
        ImGui::Text("%s", ViewNames[(int)view]);
        for (int i = 0; i < paths.size(); ++i) {
            ImGui::BulletText("%s", paths[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("削除##" + std::to_string((int)view) + "_" + std::to_string(i)).c_str())) {
                paths.erase(paths.begin() + i);
                break;
            }
        }
    }

    if (ImGui::Button("プロジェクト保存")) SaveProject("project.json");
    ImGui::SameLine();
    if (ImGui::Button("読み込み")) LoadProject("project.json");

    if (ImGui::Button("FBX出力実行")) {
        auto volumes = BuildVolumesFromImages(guiState.partViewImages);
        if (guiState.exportCombined) {
            Mesh3D combined;
            for (const auto& [part, vol] : volumes) {
                Mesh3D m = ConvertVolumeToMesh(vol, guiState.polygonCount);
                NormalizeMesh(m, guiState.scaleUnit);
                AppendMesh(combined, m);
            }
            ExportToFBX(combined, "output_combined.fbx");
        }
        else {
            for (const auto& [part, vol] : volumes) {
                Mesh3D m = ConvertVolumeToMesh(vol, guiState.polygonCount);
                NormalizeMesh(m, guiState.scaleUnit);
                std::string name = "output_" + std::to_string((int)part) + ".fbx";
                ExportToFBX(m, name.c_str());
            }
        }
    }

    ImGui::End();
}

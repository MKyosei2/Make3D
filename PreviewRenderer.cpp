#include "PreviewRenderer.h"
#include "imgui.h"
#include <cmath>

void RenderMeshWireframe(ImDrawList* drawList, const Mesh3D& mesh, ImVec2 origin, float scale) {
    auto project = [&](const Vec3& v) -> ImVec2 {
        return ImVec2(origin.x + v.x * scale, origin.y - v.y * scale); // top-down XY視点
        };

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        ImVec2 p1 = project(mesh.vertices[mesh.indices[i]]);
        ImVec2 p2 = project(mesh.vertices[mesh.indices[i + 1]]);
        ImVec2 p3 = project(mesh.vertices[mesh.indices[i + 2]]);
        drawList->AddLine(p1, p2, IM_COL32(255, 255, 0, 255));
        drawList->AddLine(p2, p3, IM_COL32(255, 255, 0, 255));
        drawList->AddLine(p3, p1, IM_COL32(255, 255, 0, 255));
    }
}

void RenderPreviewWindow(const std::vector<Mesh3D>& meshes) {
    ImGui::Begin("3Dプレビュー");

    ImVec2 canvasP0 = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 100.0f) canvasSize.x = 300;
    if (canvasSize.y < 100.0f) canvasSize.y = 300;
    ImVec2 canvasP1 = ImVec2(canvasP0.x + canvasSize.x, canvasP0.y + canvasSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasP0, canvasP1, IM_COL32(50, 50, 50, 255));
    drawList->AddRect(canvasP0, canvasP1, IM_COL32(255, 255, 255, 255));

    ImVec2 center = ImVec2((canvasP0.x + canvasP1.x) * 0.5f, (canvasP0.y + canvasP1.y) * 0.5f);
    float scale = 20.0f; // スケール係数

    for (const auto& mesh : meshes) {
        RenderMeshWireframe(drawList, mesh, center, scale);
    }

    ImGui::Dummy(canvasSize);
    ImGui::End();
}

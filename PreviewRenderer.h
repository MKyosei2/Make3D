#pragma once
#include <Windows.h>
#include "MeshGenerator.h"

class PreviewRenderer {
public:
    PreviewRenderer(HWND windowHandle);
    ~PreviewRenderer();

    void setMesh(const Mesh& mesh);
    void render();
    void rotate(float angleDelta);

private:
    HWND hwnd = nullptr;
    Mesh currentMesh;
    float angle = 0.0f;

    void drawMesh(HDC hdc);
};
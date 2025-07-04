#pragma once
#include <Windows.h>
#include "MeshGenerator.h"

class PreviewRenderer {
public:
    PreviewRenderer(HWND windowHandle);
    ~PreviewRenderer();

    void setMesh(const Mesh& mesh);
    void render();

private:
    HWND hwnd = nullptr;
    Mesh currentMesh;

    void drawMesh(HDC hdc);
};
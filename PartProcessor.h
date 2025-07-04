#pragma once
#include <Windows.h>
#include "MeshGenerator.h"

class PreviewRenderer {
public:
    PreviewRenderer(HWND hwnd);
    ~PreviewRenderer();
    void setMesh(const Mesh& mesh);
    void render();

private:
    HWND hwnd;
    Mesh currentMesh;
    void drawMesh(HDC hdc);
};
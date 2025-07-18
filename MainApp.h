#pragma once
#include <windows.h>
#include <string>
#include "PreviewRenderer.h"

enum ViewDirection {
    FRONT = 0, BACK, LEFT, RIGHT, TOP, BOTTOM
};

class MainApp
{
public:
    MainApp(HINSTANCE hInst);
    bool initialize();
    int run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool loadImageForView(ViewDirection dir, const std::wstring& imagePath);
    bool generateModel();

    HINSTANCE hInstance;
    HWND hwnd;
    HWND buttons[6];
    HWND hSilhouette;
    HWND hStatusText;
    HBITMAP images[6];
    int silhouetteThreshold;

    PreviewRenderer preview; // Å© í«â¡
};
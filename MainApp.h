#pragma once
#include <windows.h>
#include <string>

enum ViewDirection {
    FRONT = 0, BACK, LEFT, RIGHT, TOP, BOTTOM
};

class MainApp
{
public:
    MainApp(HINSTANCE hInstance);
    bool initialize();
    int run();

private:
    HINSTANCE hInstance;
    HWND hwnd;
    HWND buttons[6];
    HWND hSilhouette;
    HWND hStatusText;

    HBITMAP images[6] = {};
    int silhouetteThreshold = 64;

    bool loadImageForView(ViewDirection dir, const std::wstring& imagePath);
    bool generateModel();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

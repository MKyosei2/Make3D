#include "AppState.h"
#include <windows.h>
#include <shlwapi.h>
#include "ImageUtils.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

bool AppState::loadImages(const std::wstring& directory) {
    bool success = true;

    const wchar_t* viewNames[] = {
        L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom"
    };

    for (int i = 0; i < (int)ViewDirection::Count; ++i) {
        std::wstring path = directory + L"\\" + viewNames[i] + L".bmp";
        HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (hBitmap) {
            images[i] = hBitmap;
        }
        else {
            success = false;
        }
    }

    return success;
}

void AppState::clearImages() {
    for (int i = 0; i < (int)ViewDirection::Count; ++i) {
        if (images[i]) {
            DeleteObject(images[i]);
            images[i] = nullptr;
        }
    }
}

bool AppState::loadImageForView(ViewDirection view, const std::wstring& path) {
    Bitmap* bitmap = new Bitmap(path.c_str());
    if (bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        return false;
    }

    HBITMAP hBitmap = nullptr;
    if (bitmap->GetHBITMAP(Color::White, &hBitmap) != Ok) {
        delete bitmap;
        return false;
    }

    images[(int)view] = hBitmap;
    delete bitmap;
    return true;
}

HBITMAP AppState::getImageBitmap(ViewDirection view) const {
    return images[(int)view];
}
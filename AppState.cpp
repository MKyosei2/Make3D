#include "AppState.h"
#include <windows.h>
#include <shlwapi.h>
#include "ImageUtils.h"

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
    HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (hBitmap) {
        images[(int)view] = hBitmap;
        return true;
    }
    return false;
}
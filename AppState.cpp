#include "AppState.h"
#include <windows.h>
#include <shlwapi.h>
#include "ImageUtils.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

bool AppState::loadImages(const std::wstring& directory) {
    bool success = true;
    const wchar_t* viewNames[] = { L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom" };

    for (int i = 0; i < (int)ViewDirection::Count; ++i) {
        std::wstring path = directory + L"\\" + viewNames[i] + L".bmp";
        if (!loadImageForView((ViewDirection)i, path)) {
            MessageBoxW(nullptr, path.c_str(), L"この画像の読み込みに失敗しました", MB_ICONWARNING);
            success = false;
        }
    }
    return success;
}

HBITMAP AppState::getImageBitmap(ViewDirection view) const {
    return images[(int)view];
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
    Gdiplus::Bitmap bitmap(path.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        MessageBoxW(nullptr, (L"読み込み失敗: " + path).c_str(), L"Bitmap 読み込み失敗", MB_ICONERROR);
        images[(int)view] = nullptr;
        return false;
    }

    Gdiplus::Bitmap* clone = bitmap.Clone(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), PixelFormat32bppARGB);
    if (!clone) {
        MessageBoxW(nullptr, L"32bit変換 (Clone) に失敗しました", L"Bitmap Clone 失敗", MB_ICONERROR);
        return false;
    }

    HBITMAP hBitmap = nullptr;
    if (clone->GetHBITMAP(Gdiplus::Color(255, 255, 255), &hBitmap) != Gdiplus::Ok || !hBitmap) {
        delete clone;
        MessageBoxW(nullptr, L"HBITMAP 取得に失敗しました", L"GetHBITMAP 失敗", MB_ICONERROR);
        return false;
    }
    delete clone;

    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    if (bmp.bmWidth == 0 || bmp.bmHeight == 0) {
        DeleteObject(hBitmap);
        MessageBoxW(nullptr, L"HBITMAP サイズが無効です", L"Bitmap サイズ異常", MB_ICONERROR);
        return false;
    }

    HBITMAP hMasked = ExtractMaskFromBitmap(hBitmap);
    if (!hMasked) {
        MessageBoxW(nullptr, (L"マスク抽出失敗: " + path).c_str(), L"ExtractMaskFromBitmap() 失敗", MB_ICONERROR);
        DeleteObject(hBitmap);
        return false;
    }

    DeleteObject(hBitmap);
    if (images[(int)view]) DeleteObject(images[(int)view]);
    images[(int)view] = hMasked;

    return true;
}
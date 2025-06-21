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
            DeleteObject(images[i]); // 古いビットマップがあるなら削除
            images[i] = nullptr;     // ★ 明示的に初期化
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
    // GDI+ Bitmap 読み込み
    Bitmap* bitmap = new Bitmap(path.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        images[(int)view] = nullptr;
        return false;
    }

    HBITMAP hBitmap = nullptr;
    if (bitmap->GetHBITMAP(Color::White, &hBitmap) != Ok || !hBitmap) {
        delete bitmap;
        images[(int)view] = nullptr;
        return false;
    }
    delete bitmap;

    // 既存画像を削除（ダブルロード対策）
    if (images[(int)view]) {
        DeleteObject(images[(int)view]);
        images[(int)view] = nullptr;
    }

    // 自動マスク抽出（フォールバックあり）
    HBITMAP hMasked = ExtractMaskFromBitmap(hBitmap);
    if (hMasked) {
        DeleteObject(hBitmap);
        images[(int)view] = hMasked;
    }
    else {
        if (hBitmap)
            images[(int)view] = hBitmap;
        else
            images[(int)view] = nullptr;
    }

    return images[(int)view] != nullptr;
}

HBITMAP AppState::getImageBitmap(ViewDirection view) const {
    return images[(int)view];
}
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include "ImageUtils.h"
#include "common.h"

// ƒAƒvƒŠ‘S‘Ì‚Ìó‘Ô
struct AppState {
	HBITMAP images[(int)ViewDirection::Count];
	AlignmentParams alignments[(int)ViewDirection::Count];
	int voxelResolution = 128;
	int polygonCount = 10000;
	bool loadImages(const std::wstring& directory);
	void clearImages();
	bool loadImageForView(ViewDirection view, const std::wstring& path);
	HBITMAP getImageBitmap(ViewDirection view) const; // © ‚±‚ê‚ğ’Ç‰Á
};
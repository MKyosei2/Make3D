#pragma once

#include <string>
#include "GUIState.h"

// PNG‰æ‘œ‚ğ“Ç‚İ‚Ş
bool loadPNGImage(const std::wstring& filename, ImageData& outImage);

// ƒƒ‚ƒŠ‚ğ‰ğ•ú‚·‚é
void freeImage(ImageData& image);

#pragma once
#include <windows.h>
#include <vector>
#include "PartTypes.h"

// 1‰æ‘œ‚©‚ç•¡”‚Ì PartRegion ‚ğ’Šo‚·‚é
std::vector<PartRegion> extractRegionsFromMask(HBITMAP hBitmap);

// w’è—Ìˆæ‚É‹éŒ`˜g‚ğ•`‰æ‚·‚é
void drawRegionsToHDC(HDC hdc, const std::vector<PartRegion>& regions);

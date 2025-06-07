#pragma once
#include <windows.h>
#include "common.h"

class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();

    void render(HDC hdc, const RECT& rect);
    void setViewDirection(ViewDirection dir);

private:
    ViewDirection currentDirection;
};

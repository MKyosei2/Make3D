#pragma once
struct PNGImage {
    int width;
    int height;
    std::vector<unsigned char> pixels;
};

bool LoadPNG(const char* filename, PNGImage& outImage);

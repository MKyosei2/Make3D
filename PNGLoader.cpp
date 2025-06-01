#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "PNGLoader.h"

Image2D LoadPNG(const char* filename) {
    int w, h, n;
    uint8_t* data = stbi_load(filename, &w, &h, &n, 4);

    Image2D img;
    if (data) {
        img.width = w;
        img.height = h;
        img.pixels.assign(data, data + (w * h * 4));
        stbi_image_free(data);
    }

    return img;
}

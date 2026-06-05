#include "Make3DAdvancedCore.h"
#include "Make3DCompleteGameAssetPipeline.h"
#include "Make3DMaskRefiner.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void Set(make3d::ImageRGBA& img, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    size_t p = (static_cast<size_t>(y) * img.width + x) * 4;
    img.pixels[p] = r; img.pixels[p + 1] = g; img.pixels[p + 2] = b; img.pixels[p + 3] = 255;
}

static make3d::ImageRGBA Blank(int w = 128, int h = 128) {
    make3d::ImageRGBA img; img.width = w; img.height = h; img.pixels.assign(static_cast<size_t>(w) * h * 4, 0); return img;
}

static make3d::ImageRGBA Building() {
    auto img = Blank();
    for (int y=24;y<116;++y) for (int x=32;x<96;++x) Set(img,x,y,150,155,165);
    for (int y=16;y<28;++y) for (int x=24;x<104;++x) Set(img,x,y,105,90,80);
    for (int r=0;r<3;++r) for (int c=0;c<3;++c) for (int y=38+r*20;y<47+r*20;++y) for (int x=44+c*17;x<53+c*17;++x) Set(img,x,y,80,120,170);
    for (int y=96;y<116;++y) for (int x=59;x<69;++x) Set(img,x,y,80,65,55);
    return img;
}

static make3d::ImageRGBA Vehicle() {
    auto img = Blank();
    for (int y=58;y<86;++y) for (int x=20;x<108;++x) Set(img,x,y,180,80,70);
    for (int y=42;y<62;++y) for (int x=48;x<82;++x) Set(img,x,y,190,95,80);
    for (int y=82;y<96;++y) for (int x=30;x<48;++x) Set(img,x,y,40,40,45);
    for (int y=82;y<96;++y) for (int x=80;x<98;++x) Set(img,x,y,40,40,45);
    return img;
}

static make3d::ImageRGBA Prop() {
    auto img = Blank();
    for (int y=30;y<100;++y) for (int x=48;x<80;++x) Set(img,x,y,110,160,110);
    for (int y=24;y<40;++y) for (int x=42;x<86;++x) Set(img,x,y,130,180,130);
    for (int y=88;y<108;++y) for (int x=38;x<90;++x) Set(img,x,y,90,120,90);
    return img;
}

static bool SaveTga(const fs::path& path, const make3d::ImageRGBA& img) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary); if (!f) return false;
    unsigned char h[18] = {}; h[2]=2; h[12]=(unsigned char)(img.width&255); h[13]=(unsigned char)((img.width>>8)&255); h[14]=(unsigned char)(img.height&255); h[15]=(unsigned char)((img.height>>8)&255); h[16]=32; h[17]=8|0x20;
    f.write((const char*)h, sizeof(h));
    for (int i=0;i<img.width*img.height;++i) { size_t p=(size_t)i*4; unsigned char bgra[4]={img.pixels[p+2],img.pixels[p+1],img.pixels[p],img.pixels[p+3]}; f.write((const char*)bgra,4); }
    return true;
}

static bool RunOne(const std::string& name, const make3d::ImageRGBA& img, const fs::path& outRoot) {
    fs::path out = outRoot / name;
    SaveTga(out / (name + ".tga"), img);
    make3d::ReconstructionReport rr;
    auto mask = make3d::BuildForegroundMask(img, &rr);
    make3d::MaskRefineOptions mo; mo.keepLargestComponentOnly = true; mo.smoothingIterations = 1;
    make3d::RefineForegroundMask(mask, img.width, img.height, mo);
    make3d::AdvancedOptions ao; ao.mode = make3d::ReconstructionMode::HybridVolume; ao.quality = make3d::QualityPreset::Detailed;
    auto depth = make3d::PrepareDepth(img, std::nullopt, mask, ao, &rr);
    make3d::CompleteGameAssetOptions co; co.textureSize = 256;
    auto result = make3d::BuildCompleteGameAsset(img, depth, mask, out / "complete", co);
    return result.ok;
}

int main(int argc, char** argv) {
    fs::path out = argc >= 2 ? fs::path(argv[1]) : fs::path("complete_game_asset_suite");
    fs::create_directories(out);
    bool ok = true;
    ok = RunOne("building", Building(), out) && ok;
    ok = RunOne("vehicle", Vehicle(), out) && ok;
    ok = RunOne("prop", Prop(), out) && ok;
    std::ofstream index(out / "OPEN_THIS_FIRST.md", std::ios::binary);
    index << "# Make3D Complete Game Asset Suite\n\n";
    index << "- building/complete/make3d_complete_textured.obj\n";
    index << "- vehicle/complete/make3d_complete_textured.obj\n";
    index << "- prop/complete/make3d_complete_textured.obj\n";
    if (!ok) { std::cerr << "One or more complete asset samples failed.\n"; return 1; }
    std::cout << "Complete game asset suite generated: " << out.u8string() << "\n";
    return 0;
}

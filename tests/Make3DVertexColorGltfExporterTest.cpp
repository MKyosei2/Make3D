#include "Make3DVertexColorGltfExporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static make3d::ImageRGBA BuildImage() {
    make3d::ImageRGBA image;
    image.width = 4;
    image.height = 4;
    image.pixels.assign(4 * 4 * 4, 255);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            size_t p = (static_cast<size_t>(y) * 4 + x) * 4;
            image.pixels[p + 0] = static_cast<unsigned char>(x * 60);
            image.pixels[p + 1] = static_cast<unsigned char>(y * 60);
            image.pixels[p + 2] = 200;
            image.pixels[p + 3] = 255;
        }
    }
    return image;
}

static make3d::MeshData BuildMesh() {
    make3d::MeshData mesh;
    mesh.positions = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    mesh.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    mesh.uvs = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f
    };
    mesh.indices = {0, 1, 2};
    return mesh;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    fs::path out = fs::current_path() / "vertex_color_gltf_test";
    fs::create_directories(out);
    fs::path gltf = out / "colored.gltf";

    std::string error;
    make3d::VertexColorGltfOptions options;
    options.materialName = "UnitTestVertexColor";
    if (!make3d::ExportGLTFWithVertexColors(BuildMesh(), BuildImage(), gltf, options, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!fs::exists(gltf)) return Fail("colored glTF was not written");
    if (!fs::exists(out / "colored.bin")) return Fail("colored bin was not written");

    std::ifstream file(gltf, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (text.find("COLOR_0") == std::string::npos) return Fail("COLOR_0 accessor missing");
    if (text.find("UnitTestVertexColor") == std::string::npos) return Fail("material name missing");

    std::cout << "[PASS] Make3D vertex color glTF exporter test\n";
    return 0;
}

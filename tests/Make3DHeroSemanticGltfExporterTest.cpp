#include "Make3DHeroSemanticGltfExporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::MeshData mesh;
    mesh.positions = {
        -0.5f, 0.0f, 0.0f,
         0.5f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f
    };
    mesh.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    mesh.uvs = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    mesh.indices = {0, 1, 2};

    fs::path out = fs::current_path() / "hero_semantic_gltf_smoke";
    fs::create_directories(out);
    fs::path gltf = out / "hero_semantic.gltf";
    make3d::HeroSemanticPalette palette;
    std::string error;
    if (!make3d::ExportHeroSemanticGLTF(mesh, palette, gltf, make3d::HeroSemanticGltfOptions{}, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!fs::exists(gltf)) return Fail("gltf missing");
    if (!fs::exists(out / "hero_semantic.bin")) return Fail("bin missing");
    std::ifstream f(gltf, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (text.find("COLOR_0") == std::string::npos) return Fail("COLOR_0 missing");
    std::cout << "[PASS] Make3D hero semantic glTF exporter test\n";
    return 0;
}

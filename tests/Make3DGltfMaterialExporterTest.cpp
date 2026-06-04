#include "Make3DGltfMaterialExporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static make3d::MeshData BuildTriangle() {
    make3d::MeshData mesh;
    mesh.positions = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    mesh.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    mesh.uvs = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f
    };
    mesh.indices = {0, 1, 2};
    return mesh;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

static std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    fs::path out = fs::current_path() / "gltf_material_test";
    fs::create_directories(out);

    make3d::GltfMaterialOptions material;
    material.materialName = "TestMaterial";
    material.baseColorFactor = {0.3f, 0.6f, 0.9f, 1.0f};
    material.roughnessFactor = 0.7f;
    material.textureUri = out / "dummy_texture.png";

    std::ofstream tex(*material.textureUri, std::ios::binary);
    tex << "not-a-real-png-for-uri-test";

    std::string error;
    fs::path gltf = out / "material_test.gltf";
    if (!make3d::ExportGLTFWithMaterial(BuildTriangle(), gltf, material, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!fs::exists(gltf)) return Fail("glTF was not written");
    if (!fs::exists(out / "material_test.bin")) return Fail("glTF bin was not written");

    std::string text = ReadText(gltf);
    if (text.find("\"materials\"") == std::string::npos) return Fail("materials section missing");
    if (text.find("TestMaterial") == std::string::npos) return Fail("material name missing");
    if (text.find("baseColorFactor") == std::string::npos) return Fail("baseColorFactor missing");
    if (text.find("baseColorTexture") == std::string::npos) return Fail("baseColorTexture missing");
    if (text.find("\"images\"") == std::string::npos) return Fail("images section missing");
    if (text.find("dummy_texture.png") == std::string::npos) return Fail("texture uri missing");

    std::cout << "[PASS] glTF material exporter test\n";
    return 0;
}

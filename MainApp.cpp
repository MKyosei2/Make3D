#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <Shlwapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>
#include <sstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fbxsdk.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "libfbxsdk.lib")

std::string GetExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    return std::string(exePath);
}
std::string GetTempFBXPath(const std::string& exeDir) {
    std::ostringstream oss;
    oss << exeDir << "\\output_" << std::time(nullptr) << ".fbx";
    return oss.str();
}
void ShowError(const char* msg) {
    MessageBoxA(nullptr, msg, "Error", MB_OK | MB_ICONERROR);
}

// 3値最大・最小
unsigned char max3(unsigned char a, unsigned char b, unsigned char c) {
    unsigned char m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}
unsigned char min3(unsigned char a, unsigned char b, unsigned char c) {
    unsigned char m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

enum GrayMode {
    Gray_Luminance,
    Gray_R,
    Gray_G,
    Gray_B,
    Gray_Max,
    Gray_Min
};

bool GenerateDistanceMap(const std::string& imagePath, int& width, int& height, std::vector<float>& distanceMap, GrayMode mode = Gray_Luminance, bool invert = true) {
    int channels;
    unsigned char* image = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
    if (!image) return false;
    distanceMap.resize(width * height);
    for (int i = 0; i < width * height; ++i) {
        float gray = 0.0f;
        if (channels == 1) {
            gray = image[i] / 255.0f;
        }
        else {
            int idx = i * channels;
            unsigned char r = image[idx], g = image[idx + 1], b = image[idx + 2];
            switch (mode) {
            case Gray_Luminance: gray = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f; break;
            case Gray_R:         gray = r / 255.0f; break;
            case Gray_G:         gray = g / 255.0f; break;
            case Gray_B:         gray = b / 255.0f; break;
            case Gray_Max:       gray = max3(r, g, b) / 255.0f; break;
            case Gray_Min:       gray = min3(r, g, b) / 255.0f; break;
            }
        }
        distanceMap[i] = invert ? (1.0f - gray) : gray;
    }
    stbi_image_free(image);
    return true;
}

void ExportToFBX(const std::vector<float>& vertices, int vertexCount, const std::string& filename) {
    bool allZero = true;
    int n = (vertexCount < 100) ? vertexCount : 100;
    for (int i = 0; i < n; ++i) {
        if (vertices[i * 3] != 0.0f || vertices[i * 3 + 1] != 0.0f || vertices[i * 3 + 2] != 0.0f) {
            allZero = false;
            break;
        }
    }
    if (allZero) {
        ShowError("全頂点座標が0です。HLSLや画像・ISOLEVELを見直してください。");
        return;
    }
    std::vector<float> validVerts;
    for (int i = 0; i + 2 < vertexCount; i += 3) {
        bool valid = false;
        for (int j = 0; j < 3; ++j) {
            if (vertices[(i + j) * 3 + 0] != 0.0f ||
                vertices[(i + j) * 3 + 1] != 0.0f ||
                vertices[(i + j) * 3 + 2] != 0.0f) {
                valid = true;
                break;
            }
        }
        if (valid) {
            for (int j = 0; j < 3; ++j) {
                validVerts.push_back(vertices[(i + j) * 3 + 0]);
                validVerts.push_back(vertices[(i + j) * 3 + 1]);
                validVerts.push_back(vertices[(i + j) * 3 + 2]);
            }
        }
    }
    int validCount = (int)validVerts.size() / 3;

    FbxManager* sdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
    sdkManager->SetIOSettings(ios);
    FbxScene* scene = FbxScene::Create(sdkManager, "My Scene");

    FbxNode* rootNode = scene->GetRootNode();
    FbxMesh* mesh = FbxMesh::Create(scene, "MarchingCubesMesh");
    mesh->InitControlPoints(validCount);
    for (int i = 0; i < validCount; ++i)
        mesh->SetControlPointAt(FbxVector4(validVerts[i * 3], validVerts[i * 3 + 1], validVerts[i * 3 + 2]), i);
    for (int i = 0; i < validCount; i += 3) {
        mesh->BeginPolygon();
        mesh->AddPolygon(i);
        mesh->AddPolygon(i + 1);
        mesh->AddPolygon(i + 2);
        mesh->EndPolygon();
    }
    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(mesh);
    rootNode->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(sdkManager, "");
    bool ok = exporter->Initialize(filename.c_str(), -1, sdkManager->GetIOSettings());
    if (!ok) {
        std::string err = "FBXExporter Initialize failed: ";
        err += exporter->GetStatus().GetErrorString();
        MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
    }
    else {
        bool success = exporter->Export(scene);
        if (!success) {
            std::string err = "FBXExporter Export failed: ";
            err += exporter->GetStatus().GetErrorString();
            MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
        }
        else {
            MessageBoxA(nullptr, ("FBX Export Success!\n" + filename).c_str(), "Export", 0);
        }
    }
    exporter->Destroy();
    sdkManager->Destroy();
}

void RunMarchingCubesGPU(const std::string& imagePath, const std::string& shaderPath, const std::string& fbxPath, GrayMode mode = Gray_Luminance, bool invert = true) {
    int w, h;
    std::vector<float> distance2D;
    if (!GenerateDistanceMap(imagePath, w, h, distance2D, mode, invert)) {
        ShowError("Failed to load image.");
        return;
    }
    const int depth = 32;
    const int volumeSize = w * h * depth;
    std::vector<float> volume(volumeSize);
    for (int z = 0; z < depth; ++z)
        for (int i = 0; i < w * h; ++i)
            volume[z * w * h + i] = distance2D[i];

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context))) {
        ShowError("Failed to initialize D3D11.");
        return;
    }

    ID3D11ComputeShader* cs = nullptr;
    ID3DBlob* csBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(
        std::wstring(shaderPath.begin(), shaderPath.end()).c_str(),
        nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob, nullptr
    );
    if (FAILED(hr)) {
        ShowError("Failed to compile compute shader.");
        if (csBlob) csBlob->Release();
        return;
    }
    if (FAILED(device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs))) {
        ShowError("Failed to create compute shader.");
        csBlob->Release();
        return;
    }
    csBlob->Release();

    D3D11_BUFFER_DESC inputDesc = {};
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    inputDesc.ByteWidth = sizeof(float) * (UINT)volume.size();
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    inputDesc.StructureByteStride = sizeof(float);
    inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    D3D11_SUBRESOURCE_DATA inputData = {};
    inputData.pSysMem = volume.data();

    ID3D11Buffer* inputBuffer = nullptr;
    if (FAILED(device->CreateBuffer(&inputDesc, &inputData, &inputBuffer))) {
        ShowError("Failed to create input buffer.");
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.ElementWidth = (UINT)volume.size();

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device->CreateShaderResourceView(inputBuffer, &srvDesc, &srv))) {
        ShowError("Failed to create SRV.");
        return;
    }

    const int maxVerts = 300000;
    D3D11_BUFFER_DESC outDesc = {};
    outDesc.Usage = D3D11_USAGE_DEFAULT;
    outDesc.ByteWidth = sizeof(float) * maxVerts * 3;
    outDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    outDesc.StructureByteStride = sizeof(float) * 3;
    outDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    ID3D11Buffer* outputBuffer = nullptr;
    if (FAILED(device->CreateBuffer(&outDesc, nullptr, &outputBuffer))) {
        ShowError("Failed to create output buffer.");
        return;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = maxVerts;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

    ID3D11UnorderedAccessView* uav = nullptr;
    if (FAILED(device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &uav))) {
        ShowError("Failed to create UAV.");
        return;
    }

    UINT initialCount = 0;
    context->CSSetUnorderedAccessViews(0, 1, &uav, &initialCount);

    context->CSSetShader(cs, nullptr, 0);
    context->CSSetShaderResources(0, 1, &srv);
    context->Dispatch(w / 8, h / 8, depth / 8);

    ID3D11Buffer* countBuffer = nullptr;
    D3D11_BUFFER_DESC countDesc = {};
    countDesc.Usage = D3D11_USAGE_STAGING;
    countDesc.ByteWidth = sizeof(UINT);
    countDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    countDesc.BindFlags = 0;
    countDesc.MiscFlags = 0;
    countDesc.StructureByteStride = 0;
    if (FAILED(device->CreateBuffer(&countDesc, nullptr, &countBuffer))) {
        ShowError("Failed to create count buffer.");
        return;
    }
    context->CopyStructureCount(countBuffer, 0, uav);

    D3D11_MAPPED_SUBRESOURCE countMap = {};
    UINT vertCount = 0;
    if (SUCCEEDED(context->Map(countBuffer, 0, D3D11_MAP_READ, 0, &countMap))) {
        vertCount = *((UINT*)countMap.pData);
        context->Unmap(countBuffer, 0);
    }
    if (vertCount == 0) {
        ShowError("出力頂点数が0です。ISOLEVELや画像を見直してください。");
        return;
    }

    D3D11_BUFFER_DESC readDesc = outDesc;
    readDesc.Usage = D3D11_USAGE_STAGING;
    readDesc.BindFlags = 0;
    readDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    readDesc.MiscFlags = 0;

    ID3D11Buffer* readBuffer = nullptr;
    if (FAILED(device->CreateBuffer(&readDesc, nullptr, &readBuffer))) {
        ShowError("Failed to create readback buffer.");
        return;
    }
    context->CopyResource(readBuffer, outputBuffer);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr2 = context->Map(readBuffer, 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr2)) {
        float* data = reinterpret_cast<float*>(mapped.pData);
        int count = vertCount;
        std::vector<float> vertices(data, data + count * 3);
        context->Unmap(readBuffer, 0);

        std::ostringstream oss;
        int n = (count < 10) ? count : 10;
        oss << "先頭10頂点:\n";
        for (int i = 0; i < n; ++i)
            oss << vertices[i * 3] << "," << vertices[i * 3 + 1] << "," << vertices[i * 3 + 2] << "\n";
        MessageBoxA(nullptr, oss.str().c_str(), "頂点データチェック", MB_OK);

        ExportToFBX(vertices, count, fbxPath);
    }
    else {
        ShowError("Failed to map buffer for reading.");
    }

    if (uav) uav->Release();
    if (srv) srv->Release();
    if (inputBuffer) inputBuffer->Release();
    if (outputBuffer) outputBuffer->Release();
    if (readBuffer) readBuffer->Release();
    if (countBuffer) countBuffer->Release();
    if (cs) cs->Release();
    if (context) context->Release();
    if (device) device->Release();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::string exeDir = GetExeDirectory();
    std::string shaderPath = exeDir + "\\MarchingCubes.compute.hlsl";
    std::string imagePath = exeDir + "\\input.png";
    std::string fbxPath = GetTempFBXPath(exeDir);

    GrayMode mode = Gray_Luminance; // Gray_Luminance, Gray_R, Gray_G, Gray_B, Gray_Max, Gray_Min
    bool invert = true;             // falseも試す

    RunMarchingCubesGPU(imagePath, shaderPath, fbxPath, mode, invert);
    return 0;
}

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <fbxsdk.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "libfbxsdk.lib")

#define WIDTH 256
#define HEIGHT 256
#define DEPTH 32
#define MAX_VERTS 300000

std::vector<float> createDistanceMap(const unsigned char* img, int w, int h) {
    std::vector<float> field(WIDTH * HEIGHT * DEPTH, 0.0f);
    for (int z = 0; z < DEPTH; ++z)
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x) {
                int idx = z * WIDTH * HEIGHT + y * WIDTH + x;
                int px = x * w / WIDTH, py = y * h / HEIGHT;
                field[idx] = img[py * w + px] / 255.0f;
            }
    return field;
}

// ファイル選択ダイアログ（PNGのみ）
std::string OpenPngFileDialog() {
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "PNGファイル (*.png)\0*.png\0すべてのファイル (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "PNGファイルを選択";
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filePath);
    }
    else {
        return "";
    }
}

// FBX出力関数（省略せず前回のまま）

void ExportToFBX(const std::vector<float>& vertices, int vertCount, const std::string& filename) {
    FbxManager* lSdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    lSdkManager->SetIOSettings(ios);
    FbxScene* lScene = FbxScene::Create(lSdkManager, "Scene");

    FbxNode* lRootNode = lScene->GetRootNode();
    FbxMesh* lMesh = FbxMesh::Create(lScene, "MarchingCubesMesh");
    lMesh->InitControlPoints(vertCount);
    for (int i = 0; i < vertCount; ++i) {
        lMesh->SetControlPointAt(FbxVector4(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]), i);
    }
    for (int i = 0; i < vertCount; i += 3) {
        lMesh->BeginPolygon();
        lMesh->AddPolygon(i + 0);
        lMesh->AddPolygon(i + 1);
        lMesh->AddPolygon(i + 2);
        lMesh->EndPolygon();
    }
    FbxNode* lMeshNode = FbxNode::Create(lScene, "MeshNode");
    lMeshNode->SetNodeAttribute(lMesh);
    lRootNode->AddChild(lMeshNode);

    FbxExporter* lExporter = FbxExporter::Create(lSdkManager, "");
    if (!lExporter->Initialize(filename.c_str(), -1, lSdkManager->GetIOSettings())) {
        std::string err = "FBXExporter Initialize failed: ";
        err += lExporter->GetStatus().GetErrorString();
        MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
    }
    else {
        if (!lExporter->Export(lScene)) {
            std::string err = "FBXExporter Export failed: ";
            err += lExporter->GetStatus().GetErrorString();
            MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
        }
        else {
            MessageBoxA(nullptr, ("FBX Export Success!\n" + filename).c_str(), "Export", 0);
        }
    }
    lExporter->Destroy();
    lSdkManager->Destroy();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // PNGファイル選択
    std::string pngfile = OpenPngFileDialog();
    if (pngfile.empty()) {
        MessageBoxA(0, "PNGファイルが選択されませんでした", "Error", 0);
        return 1;
    }
    int w, h, comp;
    unsigned char* img = stbi_load(pngfile.c_str(), &w, &h, &comp, 1);
    if (!img) {
        MessageBoxA(0, "PNGファイルが読めません", "Error", 0);
        return 1;
    }
    auto distanceMap = createDistanceMap(img, w, h);
    stbi_image_free(img);

    // D3D11初期化、バッファ生成、ComputeShader実行（前回と同じ）

    D3D_FEATURE_LEVEL fl;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &device, &fl, &context);

    D3D11_BUFFER_DESC bufDesc = {};
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.ByteWidth = sizeof(float) * distanceMap.size();
    bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufDesc.StructureByteStride = sizeof(float);
    bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = distanceMap.data();
    ID3D11Buffer* volBuf;
    device->CreateBuffer(&bufDesc, &srd, &volBuf);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = distanceMap.size();
    ID3D11ShaderResourceView* volSRV;
    device->CreateShaderResourceView(volBuf, &srvDesc, &volSRV);

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(float) * 3 * MAX_VERTS;
    vbd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    vbd.StructureByteStride = sizeof(float) * 3;
    vbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    ID3D11Buffer* vertBuf;
    device->CreateBuffer(&vbd, nullptr, &vertBuf);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = MAX_VERTS;
    ID3D11UnorderedAccessView* vertUAV;
    device->CreateUnorderedAccessView(vertBuf, &uavDesc, &vertUAV);

    UINT zero = 0;
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(UINT);
    cbd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    cbd.StructureByteStride = sizeof(UINT);
    cbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    D3D11_SUBRESOURCE_DATA csd = {};
    csd.pSysMem = &zero;
    ID3D11Buffer* counterBuf;
    device->CreateBuffer(&cbd, &csd, &counterBuf);

    D3D11_UNORDERED_ACCESS_VIEW_DESC cuavDesc = {};
    cuavDesc.Format = DXGI_FORMAT_UNKNOWN;
    cuavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    cuavDesc.Buffer.FirstElement = 0;
    cuavDesc.Buffer.NumElements = 1;
    ID3D11UnorderedAccessView* counterUAV;
    device->CreateUnorderedAccessView(counterBuf, &cuavDesc, &counterUAV);

    ID3DBlob* csBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(
        L"MarchingCubes.compute.hlsl",
        nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob, nullptr
    );
    if (FAILED(hr)) {
        MessageBoxA(0, "HLSLのコンパイル失敗", "Error", 0);
        return 1;
    }
    ID3D11ComputeShader* cs;
    device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs);

    ID3D11ShaderResourceView* srvs[1] = { volSRV };
    ID3D11UnorderedAccessView* uavs[2] = { vertUAV, counterUAV };
    context->CSSetShaderResources(0, 1, srvs);
    context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
    context->CSSetShader(cs, nullptr, 0);

    context->Dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, (DEPTH + 7) / 8);

    UINT vertCount = 0;
    D3D11_BUFFER_DESC rdDesc = {};
    rdDesc.Usage = D3D11_USAGE_STAGING;
    rdDesc.ByteWidth = sizeof(UINT);
    rdDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    rdDesc.StructureByteStride = sizeof(UINT);
    rdDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    ID3D11Buffer* counterReadBuf;
    device->CreateBuffer(&rdDesc, nullptr, &counterReadBuf);
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->CopyResource(counterReadBuf, counterBuf);
    context->Map(counterReadBuf, 0, D3D11_MAP_READ, 0, &mapped);
    vertCount = *(UINT*)mapped.pData;
    context->Unmap(counterReadBuf, 0);

    std::vector<float> vertices(vertCount * 3);
    D3D11_BUFFER_DESC vbReadDesc = {};
    vbReadDesc.Usage = D3D11_USAGE_STAGING;
    vbReadDesc.ByteWidth = sizeof(float) * 3 * vertCount;
    vbReadDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    vbReadDesc.StructureByteStride = sizeof(float) * 3;
    vbReadDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    ID3D11Buffer* vertReadBuf;
    device->CreateBuffer(&vbReadDesc, nullptr, &vertReadBuf);
    context->CopyResource(vertReadBuf, vertBuf);
    context->Map(vertReadBuf, 0, D3D11_MAP_READ, 0, &mapped);
    memcpy(vertices.data(), mapped.pData, sizeof(float) * 3 * vertCount);
    context->Unmap(vertReadBuf, 0);

    // FBX出力
    char filename[256];
    std::time_t t = std::time(nullptr);
    sprintf(filename, "output_%lld.fbx", (long long)t);
    ExportToFBX(vertices, vertCount, filename);

    MessageBoxA(0, "完了", "OK", 0);
    return 0;
}

#include "PreviewRenderer.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

PreviewRenderer::PreviewRenderer() :
    device(nullptr), context(nullptr), swapChain(nullptr),
    renderTargetView(nullptr), vertexBuffer(nullptr), indexBuffer(nullptr)
{
}

PreviewRenderer::~PreviewRenderer()
{
    Shutdown();
}

bool PreviewRenderer::Initialize(HWND hwnd)
{
    this->hwnd = hwnd;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context);

    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    return true;
}

void PreviewRenderer::CreateBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    if (vertexBuffer) vertexBuffer->Release();
    if (indexBuffer) indexBuffer->Release();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = UINT(sizeof(Vertex) * vertices.size());
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertices.data();
    device->CreateBuffer(&vbd, &vinitData, &vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = UINT(sizeof(unsigned int) * indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = indices.data();
    device->CreateBuffer(&ibd, &iinitData, &indexBuffer);
}

void PreviewRenderer::Render(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    CreateBuffers(vertices, indices);

    float clearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
    context->ClearRenderTargetView(renderTargetView, clearColor);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ここでシェーダーや変換行列をセットすれば、レンダリング可能

    swapChain->Present(1, 0);
}

void PreviewRenderer::Shutdown()
{
    if (vertexBuffer) vertexBuffer->Release();
    if (indexBuffer) indexBuffer->Release();
    if (renderTargetView) renderTargetView->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
}

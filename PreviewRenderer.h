#pragma once
#include <vector>
#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")

class PreviewRenderer
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };

    PreviewRenderer();
    ~PreviewRenderer();

    bool Initialize(HWND hwnd);
    void Render(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void Shutdown();

private:
    HWND hwnd;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;

    void CreateBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
};

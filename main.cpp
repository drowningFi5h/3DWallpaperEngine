#include <Windows.h>
#include <iostream>
#include <deque>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#include <DirectXMath.h>
#include <filesystem>
using namespace DirectX;

//------------------------------------------------------------------------------------------------------------------------------------------------

// LAYERS
#define MAX_LAYERS 4

// GLobals
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;
ID3D11ShaderResourceView* g_textureViews[MAX_LAYERS] = { nullptr };
ID3D11Buffer* g_vertexBuffer = nullptr;
ID3D11VertexShader* g_vertexShader = nullptr;
ID3D11PixelShader* g_pixelShader = nullptr;
ID3D11InputLayout* g_inputLayout = nullptr;
ID3D11Buffer* g_constantBuffer = nullptr;
ID3D11SamplerState* g_samplerState = nullptr;

bool g_use3DParallax = false;


float position = 0.5f; // Default head position

struct Vertex {
    float x, y; // Position
    float u, v; // Texture coordinates (UV)
};


struct ConstantBuffer {
    XMFLOAT2 offset;         // Head position offset
    float parallaxIntensity;
    float use3DParallax;     //toggle between 2D/3D
};

//------------------------------------------------------------------------------------------------------------------------------------------------
// HEAD POSITION AND SMOOTHING THE ANGLES

float ReadHeadPosition() {
    //SHARED MEMORY
    HANDLE hMap = OpenFileMapping(
        FILE_MAP_READ,
        FALSE,
        "Global\\HeadPosition"
    );
    if (hMap == NULL) {
        std::cerr << "Failed to open shared memory (Error: " << GetLastError() << ")" << std::endl;
        return 0.5f;
    }

    float* headX = (float*)MapViewOfFile(
        hMap,
        FILE_MAP_READ,
        0, 0, 4  // Offset
    );


    if (headX == NULL) {
        std::cerr << "Failed to map view (Error: " << GetLastError() << ")" << std::endl;
        CloseHandle(hMap);
        return 0.0f;
    }

    float position = *headX;

    UnmapViewOfFile(headX);
    CloseHandle(hMap);

    return position;
}

std::deque<float> headPositions;

float GetSmoothedHeadPosition() {
    if (headPositions.size() >= 5) {
        headPositions.pop_front();
    }
    float currentPosition = ReadHeadPosition();
    headPositions.push_back(currentPosition);

    float sum = 0.0f;
    for (float pos : headPositions) {
        sum += pos;
    }
    return sum / headPositions.size();
}


//---------------------------------------------------------------------------------------------------------------------------

bool LoadTexture(const char* filename, int layerIndex) {
    if (layerIndex < 0 || layerIndex >= MAX_LAYERS) {
        std::cerr << "ERROR: Invalid layer index: " << layerIndex << std::endl;
        return false;
    }

    if (!std::filesystem::exists(filename)) {
        std::cerr << "ERROR texture file not found: " << filename << std::endl;
        return false;
    }

    int width, height, channels;
    unsigned char* pixels = stbi_load(filename, &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "ERROR: stbi_load failed for: " << filename
                  << " (Reason: " << stbi_failure_reason() << ")" << std::endl;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {0};
    initData.pSysMem = pixels;
    initData.SysMemPitch = width * 4;

    ID3D11Texture2D* texture;
    HRESULT hr = g_device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create texture (Error: " << hr << ")" << std::endl;
        stbi_image_free(pixels);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_device->CreateShaderResourceView(texture, &srvDesc, &g_textureViews[layerIndex]);
    texture->Release();

    if (FAILED(hr)) {
        std::cerr << "Failed to create shader resource view (Error: " << hr << ")" << std::endl;
        stbi_image_free(pixels);
        return false;
    }

    stbi_image_free(pixels);
    std::cout << "Successfully loaded texture: " << filename << " into layer " << layerIndex << std::endl;
    return true;
}


//------------------------------------------------------------------------------------------------------------------------------------------------
// PARALLAX LAYERS LOADING

bool LoadAllLayers(const char* baseLayerPath, const char* midLayerPath, const char* topLayerPath, const char* depthMapPath) {
    bool success = true;

    success &= LoadTexture(topLayerPath, 0);
    success &= LoadTexture(midLayerPath, 1);
    success &= LoadTexture(baseLayerPath, 2);
    success &= LoadTexture(depthMapPath, 3);
    return success;
}


//------------------------------------------------------------------------------------------------------------------------------------------------
// FULLSCREEN QUAD CREATION

bool CreateFullscreenQuad() {
    Vertex vertices[] = {
        {-1.0f,  1.0f, 0.0f, 0.0f}, // Top-left
        { 1.0f,  1.0f, 1.0f, 0.0f}, // Top-right
        {-1.0f, -1.0f, 0.0f, 1.0f}, // Bottom-left
        { 1.0f, -1.0f, 1.0f, 1.0f}  // Bottom-right
    };
    D3D11_BUFFER_DESC vertexBufferDesc = {0};
    vertexBufferDesc.ByteWidth = sizeof(vertices);
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {0};
    initData.pSysMem = vertices;

    HRESULT hr = g_device->CreateBuffer(&vertexBufferDesc, &initData, &g_vertexBuffer);
    if (FAILED(hr)) {
        std::cerr << "Failed to create vertex buffer (Error: " << hr << ")" << std::endl;
        return false;
    }
    return true;
}


//------------------------------------------------------------------------------------------------------------------------------------------------
// SHADER COMPILATION AND SETUP

bool CompileShaders() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    if (!std::filesystem::exists(L"shader.hlsl")) {
        std::cerr << "shader.hlsl not found!" << std::endl;
        return false;
    }

    HRESULT hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VSmain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "Vertex Shader Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
            errorBlob->Release();
        }
        return false;
    }

    hr = g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create vertex shader" << std::endl;
        vsBlob->Release();
        return false;
    }

    // INPUT LAYOUT
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = g_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
    if (FAILED(hr)) {
        std::cerr << "Failed to create input layout" << std::endl;
        vsBlob->Release();
        return false;
    }
    vsBlob->Release();

    // PIXEL SHADER CREATION AND COMPILATION
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PSmain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "Pixel Shader Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
            errorBlob->Release();
        }
        return false;
    }

    hr = g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create pixel shader" << std::endl;
        psBlob->Release();
        return false;
    }
    psBlob->Release();

    // SAMPLE STATE
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = g_device->CreateSamplerState(&sampDesc, &g_samplerState);
    if (FAILED(hr)) {
        std::cerr << "Failed to create sampler state" << std::endl;
        return false;
    }

    g_context->PSSetSamplers(0, 1, &g_samplerState);

    // CONSTANT BUFFER
    D3D11_BUFFER_DESC cbDesc = {0};
    cbDesc.ByteWidth = sizeof(ConstantBuffer); // 4
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = g_device->CreateBuffer(&cbDesc, nullptr, &g_constantBuffer);
    if (FAILED(hr)) {
        std::cerr << "Failed to create constant buffer" << std::endl;
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------------
// RENDERING FRAME

void RenderFrame(float position) {
    g_context->OMSetRenderTargets(1, &g_renderTargetView, nullptr);

    float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    g_context->ClearRenderTargetView(g_renderTargetView, color);

    // Map constant buffer and update with head position data
    D3D11_MAPPED_SUBRESOURCE mapped;
    g_context->Map(g_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    ConstantBuffer cb;
    cb.offset = XMFLOAT2((position - 0.5f) * 0.4f, 0.0f); // Horizontal head tracking offset
    cb.parallaxIntensity = 1.0f; // PARALLAX STRENGTH
    cb.use3DParallax = g_use3DParallax ? 1.0f : 0.0f;

    memcpy(mapped.pData, &cb, sizeof(ConstantBuffer));
    g_context->Unmap(g_constantBuffer, 0);

    // RENDERING PIPELINE
    g_context->VSSetShader(g_vertexShader, nullptr, 0);
    g_context->PSSetShader(g_pixelShader, nullptr, 0);
    g_context->IASetInputLayout(g_inputLayout);
    g_context->VSSetConstantBuffers(0, 1, &g_constantBuffer);
    g_context->PSSetConstantBuffers(0, 1, &g_constantBuffer);

    // INIT TEXTURE LAYERS
    g_context->PSSetShaderResources(0, MAX_LAYERS, g_textureViews);

    // INIT SAMPLE STATE
    g_context->PSSetSamplers(0, 1, &g_samplerState);

    // INIT FULLSCREEN QUAD
    UINT stride = sizeof(Vertex);
    UINT vbOffset = 0;
    g_context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &stride, &vbOffset);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g_context->Draw(4, 0);

    g_swapchain->Present(1, 0); // VSYNC
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}


//------------------------------------------------------------------------------------------------------------------------------------------------
// INITIALIZE DIRECT3D

bool InitializeD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount=1;
    scd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow=hwnd;
    scd.SampleDesc.Count=1;
    scd.Windowed=TRUE;
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &g_swapchain,
        &g_device,
        nullptr,
        &g_context
        );

    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"DirectX3D init failed", L"Error", MB_OK);
        return false;
    }

    ID3D11Texture2D* pbackBuffer;
    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pbackBuffer);
    g_device->CreateRenderTargetView(pbackBuffer, nullptr, &g_renderTargetView);
    pbackBuffer->Release();

    if (!CreateFullscreenQuad()) {
        std::cerr << "Failed to create fullscreen quad" << std::endl;
        MessageBoxW(hwnd, L"Failed to create fullscreen quad", L"Error", MB_OK);
        return false;
    }

    if (!CompileShaders()) {
        std::cerr << "Failed to compile shaders" << std::endl;
        MessageBoxW(hwnd, L"Failed to compile shaders", L"Error", MB_OK);
        return false;
    }

    if (!LoadAllLayers(
        "C:\\PROJECTS\\ENGINE\\Wallpapers\\back.png",
        "C:\\PROJECTS\\ENGINE\\Wallpapers\\middle.png",
        "C:\\PROJECTS\\ENGINE\\Wallpapers\\front.png",
        "C:\\PROJECTS\\ENGINE\\Wallpapers\\depth_map.png"
    )) {
        std::cerr << "Failed to load texture layers" << std::endl;
        MessageBoxW(hwnd, L"Failed to load texture layers", L"Error", MB_OK);
        return false;
    }

    D3D11_VIEWPORT viewport = {0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f};
    g_context->RSSetViewports(1, &viewport);
    return true;
}


//------------------------------------------------------------------------------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    std::cerr << "Current Directory: " << currentDir << std::endl;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "D3DWindowClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        "D3DWindowClass",
        "DirectX Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);

    if (!InitializeD3D(hwnd)) {
        std::cerr << "Failed to initialize Direct3D" << std::endl;
        return -1;
    }

    std::cout << "Parallax Wallpaper Controls:" << std::endl;
    std::cout << "  Space - Toggle between 2D/3D parallax modes" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_SPACE) {
                    // Toggle between 2D and 3D parallax modes
                    g_use3DParallax = !g_use3DParallax;
                    std::cout << "Switched to " << (g_use3DParallax ? "3D" : "2D") << " parallax mode" << std::endl;
                }
                else if (msg.wParam == VK_ESCAPE) {
                    PostQuitMessage(0);
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            position = GetSmoothedHeadPosition();
            RenderFrame(position);
        }
    }

    // Cleanup
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (g_textureViews[i]) g_textureViews[i]->Release();
    }

    if (g_vertexBuffer) g_vertexBuffer->Release();
    if (g_vertexShader) g_vertexShader->Release();
    if (g_pixelShader) g_pixelShader->Release();
    if (g_inputLayout) g_inputLayout->Release();
    if (g_constantBuffer) g_constantBuffer->Release();
    if (g_samplerState) g_samplerState->Release();

    if (g_renderTargetView) g_renderTargetView->Release();
    if (g_swapchain) g_swapchain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();

    return 0;
}

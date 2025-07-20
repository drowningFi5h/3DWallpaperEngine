#include <Windows.h>
#include <iostream>
#include <deque>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")


float ReadHeadPosition() {
    // Open the shared memory
    HANDLE hMap = OpenFileMapping(
        FILE_MAP_READ,
        FALSE,
        "Global\\HeadPosition"
    );
    if (hMap == NULL) {
        std::cerr << "Failed to open shared memory (Error: " << GetLastError() << ")" << std::endl;
        return 0.5f;
    }

    // Map to the floating pointer
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



ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;
ID3D11ShaderResourceView* g_srv = nullptr;

bool InitializeD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount=1;
    scd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow=hwnd;
    scd.SampleDesc.Count=1;
    scd.Windowed=TRUE;


    D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &g_swapchain,
        &g_device,
        nullptr,
        &g_context
        );

    ID3D11Texture2D* pbackBuffer;
    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pbackBuffer);
    g_device->CreateRenderTargetView(pbackBuffer, nullptr, &g_renderTargetView);
    pbackBuffer->Release();

    return true;

}

void RenderFrame() {
    float headPosition = GetSmoothedHeadPosition();
    std::cout << "Smoothed Head Position: " << headPosition << std::endl;

    float color[4] = {headPosition, 0.2f, 0.4f, 1.0f};

    g_context->ClearRenderTargetView(g_renderTargetView, color);
    g_swapchain->Present(1, 0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
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

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            RenderFrame();
            float headPosition = ReadHeadPosition();
            std::cout << "Head Position: " << headPosition << std::endl;
        }
    }

    g_renderTargetView->Release();
    g_swapchain->Release();
    g_context->Release();
    g_device->Release();

    return 0;
}

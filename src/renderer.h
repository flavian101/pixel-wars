#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// One colored vertex in world-space coordinates.
struct Vertex {
    float x, y;
    float r, g, b, a;
};

// Minimal D3D11 renderer: clears, accumulates colored quads into a dynamic
// vertex buffer, and presents. No textures yet -- everything is a colored quad.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates device, swap chain, render target, pipeline. Returns false on failure.
    bool initialize(HWND hwnd, int width, int height);

    // Call when the window resizes (WM_SIZE).
    void resize(int width, int height);

    // Frame flow: beginFrame() -> drawQuad(...) x N -> endFrame()
    void beginFrame(float clearR, float clearG, float clearB);
    void drawQuad(float cx, float cy, float halfSize, float r, float g, float b, float a = 1.0f);
    void endFrame();

    // Post-present cleanup and book-keeping (e.g. age off-screen enemies).
    void postFrame() {}

private:
    bool createDeviceAndSwapChain(HWND hwnd, int width, int height);
    bool createBackBufferViews();
    bool createPipeline();
    bool createDynamicBuffers();

    void releaseBackBufferViews();

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain>         swapChain_;
    ComPtr<ID3D11RenderTargetView> backBufferRTV_;

    ComPtr<ID3D11VertexShader>     vertexShader_;
    ComPtr<ID3D11PixelShader>      pixelShader_;
    ComPtr<ID3D11InputLayout>      inputLayout_;
    ComPtr<ID3D11Buffer>           vertexBuffer_;
    ComPtr<ID3D11Buffer>           constantBuffer_;

    int width_  = 0;
    int height_ = 0;

    std::vector<Vertex> quadVerts_;     // accumulated this frame
    static constexpr size_t MAX_QUADS = 4096;
};

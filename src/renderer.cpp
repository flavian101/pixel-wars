#include "renderer.h"
#include "constants.h"
#include <d3dcompiler.h>
#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Shaders (compiled at runtime to keep the project dependency-free)
// ---------------------------------------------------------------------------
static const char* kShaderSource = R"(
cbuffer FrameCB : register(b0)
{
    float2 invHalfWorld; // 2/worldWidth, 2/worldHeight
};

struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };

VSOut VSMain(VSIn input)
{
    VSOut o;
    // world (0..W, 0..H), y-down  ->  NDC (-1..1), y-up
    o.pos = float4(input.pos.x * invHalfWorld.x - 1.0f,
                   1.0f - input.pos.y * invHalfWorld.y, 0.0f, 1.0f);
    o.col = input.col;
    return o;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    return input.col;
}
)";

struct FrameCB {
    float invHalfWorldX;
    float invHalfWorldY;
    float pad0, pad1; // pad to 16 bytes
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool Renderer::initialize(HWND hwnd, int width, int height) {
    width_  = width;
    height_ = height;

    if (!createDeviceAndSwapChain(hwnd, width, height)) return false;
    if (!createBackBufferViews())                       return false;
    if (!createPipeline())                              return false;
    if (!createDynamicBuffers())                        return false;
    return true;
}

bool Renderer::createDeviceAndSwapChain(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount                        = 2;
    scd.BufferDesc.Width                   = width;
    scd.BufferDesc.Height                  = height;
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hwnd;
    scd.SampleDesc.Count                   = 1;
    scd.Windowed                           = TRUE;
    scd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, 1, D3D11_SDK_VERSION,
        &scd, &swapChain_, &device_, nullptr, &context_);

    if (FAILED(hr)) {
        // Retry without the debug layer (not installed on many machines).
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, 1, D3D11_SDK_VERSION,
            &scd, &swapChain_, &device_, nullptr, &context_);
    }
    return SUCCEEDED(hr);
}

bool Renderer::createBackBufferViews() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferRTV_))) return false;
    return true;
}

void Renderer::releaseBackBufferViews() {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    backBufferRTV_.Reset();
}

void Renderer::resize(int width, int height) {
    if (!swapChain_ || width == 0 || height == 0) return;
    width_ = width;
    height_ = height;
    releaseBackBufferViews();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createBackBufferViews();
}

bool Renderer::createPipeline() {
    ComPtr<ID3DBlob> vsBlob, psBlob, error;

    if (FAILED(D3DCompile(kShaderSource, strlen(kShaderSource), nullptr, nullptr, nullptr,
                          "VSMain", "vs_5_0", 0, 0, &vsBlob, &error)))
        return false;
    if (FAILED(D3DCompile(kShaderSource, strlen(kShaderSource), nullptr, nullptr, nullptr,
                          "PSMain", "ps_5_0", 0, 0, &psBlob, &error)))
        return false;

    if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                           nullptr, &vertexShader_)))
        return false;
    if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                          nullptr, &pixelShader_)))
        return false;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,                D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float)*2,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(device_->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
                                          vsBlob->GetBufferSize(), &inputLayout_)))
        return false;
    return true;
}

bool Renderer::createDynamicBuffers() {
    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth      = static_cast<UINT>(sizeof(Vertex) * MAX_QUADS * 6);
    vbd.Usage          = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&vbd, nullptr, &vertexBuffer_))) return false;

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth      = sizeof(FrameCB);
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&cbd, nullptr, &constantBuffer_))) return false;
    return true;
}

void Renderer::beginFrame(float r, float g, float b) {
    float clear[4] = { r, g, b, 1.0f };
    context_->OMSetRenderTargets(1, backBufferRTV_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(backBufferRTV_.Get(), clear);

    D3D11_VIEWPORT vp{};
    vp.Width    = static_cast<float>(width_);
    vp.Height   = static_cast<float>(height_);
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    quadVerts_.clear();
}

void Renderer::drawQuad(float cx, float cy, float halfSize,
                        float r, float g, float b, float a) {
    if (quadVerts_.size() + 6 > MAX_QUADS * 6) return;

    float l = cx - halfSize, rt = cx + halfSize;
    float t = cy - halfSize, bt = cy + halfSize;
    auto V = [&](float x, float y) { return Vertex{ x, y, r, g, b, a }; };

    // two triangles
    quadVerts_.insert(quadVerts_.end(), {
        V(l,t), V(rt,t), V(l,bt),
        V(l,bt), V(rt,t), V(rt,bt) });
}

void Renderer::endFrame() {
    if (!quadVerts_.empty()) {
        // Upload quads
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, quadVerts_.data(), quadVerts_.size() * sizeof(Vertex));
            context_->Unmap(vertexBuffer_.Get(), 0);
        }

        // Upload frame constants
        if (SUCCEEDED(context_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            FrameCB cb{};
            cb.invHalfWorldX = 2.0f / WORLD_WIDTH;
            cb.invHalfWorldY = 2.0f / WORLD_HEIGHT;
            memcpy(mapped.pData, &cb, sizeof(cb));
            context_->Unmap(constantBuffer_.Get(), 0);
        }

        UINT stride = sizeof(Vertex), offset = 0;
        context_->IASetInputLayout(inputLayout_.Get());
        context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);

        context_->Draw(static_cast<UINT>(quadVerts_.size()), 0);
    }
    swapChain_->Present(1, 0);
}

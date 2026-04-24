#include "HealthBarRenderer.h"
#include <d3dcompiler.h>
#include <cstring>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

template <class T>
static void ReleaseCOM(T*& p)
{
    if (p) { p->Release(); p = nullptr; }
}

static std::wstring ToWidePath(const char* path)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wide[0], count);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
}

static bool CompileShaderFile(const char* path, const char* entry, const char* target, ID3DBlob** blob)
{
    ID3DBlob* errors = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    std::string resolved = path;
    std::wstring wide = ToWidePath(resolved.c_str());
    HRESULT hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, blob, &errors);
    if (FAILED(hr))
    {
        if (errors) { errors->Release(); errors = nullptr; }
        std::string name = path;
        size_t slash = name.find_last_of("\\/");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        resolved = "assets\\Shaders\\" + name;
        wide = ToWidePath(resolved.c_str());
        hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target, flags, 0, blob, &errors);
    }
    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(nullptr, (const char*)errors->GetBufferPointer(), path, MB_OK);
            errors->Release();
        }
        return false;
    }
    if (errors) errors->Release();
    return true;
}

HealthBarRenderer::HealthBarRenderer()
    : m_pDev(nullptr), m_pCtx(nullptr), m_pVS(nullptr), m_pPS(nullptr), m_pLayout(nullptr),
      m_pVB(nullptr), m_pCB(nullptr), m_pAlphaBlend(nullptr), m_pDepthOff(nullptr),
      m_width(0), m_height(0)
{
}

HealthBarRenderer::~HealthBarRenderer()
{
    Shutdown();
}

bool HealthBarRenderer::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width, int height, const char* shaderPath)
{
    m_pDev = dev;
    m_pCtx = ctx;
    m_width = width;
    m_height = height;

    if (!CompileShaders(shaderPath)) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(QuadVertex) * 6;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(m_pDev->CreateBuffer(&bd, nullptr, &m_pVB))) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(HealthBarCB);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDev->CreateBuffer(&cbd, nullptr, &m_pCB))) return false;

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_pDev->CreateBlendState(&blend, &m_pAlphaBlend))) return false;

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    return SUCCEEDED(m_pDev->CreateDepthStencilState(&depth, &m_pDepthOff));
}

bool HealthBarRenderer::CompileShaders(const char* shaderPath)
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    if (!CompileShaderFile(shaderPath, "VS_HealthBar", "vs_4_0", &vsBlob)) return false;
    if (!CompileShaderFile(shaderPath, "PS_HealthBar", "ps_4_0", &psBlob)) { vsBlob->Release(); return false; }

    HRESULT hr = m_pDev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pVS);
    if (FAILED(hr)) return false;
    hr = m_pDev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pPS);
    if (FAILED(hr)) return false;

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = m_pDev->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pLayout);
    vsBlob->Release();
    psBlob->Release();
    return SUCCEEDED(hr);
}

void HealthBarRenderer::Shutdown()
{
    ReleaseCOM(m_pDepthOff);
    ReleaseCOM(m_pAlphaBlend);
    ReleaseCOM(m_pCB);
    ReleaseCOM(m_pVB);
    ReleaseCOM(m_pLayout);
    ReleaseCOM(m_pPS);
    ReleaseCOM(m_pVS);
}

void HealthBarRenderer::Render(float x0, float y0, float x1, float y1, float progress)
{
    if (!m_pCtx || !m_pVB) return;

    QuadVertex verts[6] =
    {
        { x0, y0, 0, 0 }, { x1, y0, 1, 0 }, { x0, y1, 0, 1 },
        { x1, y0, 1, 0 }, { x1, y1, 1, 1 }, { x0, y1, 0, 1 },
    };

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(m_pCtx->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, verts, sizeof(verts));
    m_pCtx->Unmap(m_pVB, 0);

    HealthBarCB cb = {};
    cb.screenParams = D3DXVECTOR4((float)m_width, (float)m_height, 0, 0);
    cb.data = D3DXVECTOR4(progress, 0, 0, 0);
    m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);

    UINT stride = sizeof(QuadVertex);
    UINT offset = 0;
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pCtx->IASetInputLayout(m_pLayout);
    m_pCtx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
    m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCtx->VSSetShader(m_pVS, nullptr, 0);
    m_pCtx->VSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetShader(m_pPS, nullptr, 0);
    m_pCtx->PSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->OMSetBlendState(m_pAlphaBlend, blendFactor, 0xffffffff);
    m_pCtx->OMSetDepthStencilState(m_pDepthOff, 0);
    m_pCtx->Draw(6, 0);
    m_pCtx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pCtx->OMSetDepthStencilState(nullptr, 0);
}

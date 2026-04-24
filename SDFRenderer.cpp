#include "SDFRenderer.h"
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
    std::wstring wide = ToWidePath(path);
    HRESULT hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, blob, &errors);
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

SDFRenderer::SDFRenderer()
    : m_pDev(nullptr), m_pCtx(nullptr), m_pAtlas(nullptr), m_pVS(nullptr), m_pLayout(nullptr),
      m_pVB(nullptr), m_pCB(nullptr), m_pSampler(nullptr), m_pAlphaBlend(nullptr), m_pDepthOff(nullptr),
      m_vertexCapacity(0), m_width(0), m_height(0)
{
    for (int i = 0; i < SDF_EFFECT_COUNT; i++) m_pPS[i] = nullptr;
}

SDFRenderer::~SDFRenderer()
{
    Shutdown();
}

bool SDFRenderer::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, SDFAtlas* atlas,
                       int width, int height, const char* shaderPath)
{
    m_pDev = dev;
    m_pCtx = ctx;
    m_pAtlas = atlas;
    m_width = width;
    m_height = height;

    if (!CompileShaders(shaderPath)) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(SDFCB);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDev->CreateBuffer(&cbd, nullptr, &m_pCB))) return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_pDev->CreateSamplerState(&sd, &m_pSampler))) return false;

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_pDev->CreateBlendState(&bd, &m_pAlphaBlend))) return false;

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(m_pDev->CreateDepthStencilState(&dd, &m_pDepthOff))) return false;

    return true;
}

bool SDFRenderer::CompileShaders(const char* shaderPath)
{
    ID3DBlob* vsBlob = nullptr;
    if (!CompileShaderFile(shaderPath, "VS_SDF", "vs_4_0", &vsBlob)) return false;
    if (FAILED(m_pDev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pVS)))
    {
        vsBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    HRESULT hr = m_pDev->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pLayout);
    vsBlob->Release();
    if (FAILED(hr)) return false;

    const char* psEntries[SDF_EFFECT_COUNT] =
    {
        "PS_SDFSimple",
        "PS_SDFOutline",
        "PS_SDFGlow",
        "PS_SDFOutlineGlow"
    };

    for (int i = 0; i < SDF_EFFECT_COUNT; i++)
    {
        ID3DBlob* psBlob = nullptr;
        if (!CompileShaderFile(shaderPath, psEntries[i], "ps_4_0", &psBlob)) return false;
        hr = m_pDev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pPS[i]);
        psBlob->Release();
        if (FAILED(hr)) return false;
    }
    return true;
}

void SDFRenderer::Shutdown()
{
    for (int i = 0; i < SDF_EFFECT_COUNT; i++) ReleaseCOM(m_pPS[i]);
    ReleaseCOM(m_pDepthOff);
    ReleaseCOM(m_pAlphaBlend);
    ReleaseCOM(m_pSampler);
    ReleaseCOM(m_pCB);
    ReleaseCOM(m_pVB);
    ReleaseCOM(m_pLayout);
    ReleaseCOM(m_pVS);
    m_vertexCapacity = 0;
    m_entries.clear();
}

SDFTextParams& SDFRenderer::Add(const char* text, int x, int y, float fontSize, const float color[4])
{
    SDFTextParams p;
    strncpy_s(p.text, sizeof(p.text), text, _TRUNCATE);
    p.posX = x;
    p.posY = y;
    p.fontSize = fontSize;
    if (color)
    {
        p.color[0] = color[0]; p.color[1] = color[1];
        p.color[2] = color[2]; p.color[3] = color[3];
    }
    m_entries.push_back(p);
    return m_entries.back();
}

void SDFRenderer::Remove(size_t index)
{
    if (index < m_entries.size())
        m_entries.erase(m_entries.begin() + index);
}

void SDFRenderer::Clear()
{
    m_entries.clear();
}

bool SDFRenderer::EnsureVertexCapacity(size_t vertexCount)
{
    if (vertexCount <= m_vertexCapacity) return true;

    ReleaseCOM(m_pVB);
    m_vertexCapacity = vertexCount + 256;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = (UINT)(m_vertexCapacity * sizeof(QuadVertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(m_pDev->CreateBuffer(&bd, nullptr, &m_pVB));
}

void SDFRenderer::BuildQuads(const SDFTextParams& p, std::vector<QuadVertex>& verts)
{
    wchar_t wtext[1024];
    MultiByteToWideChar(CP_UTF8, 0, p.text, -1, wtext, 1024);

    float scale = p.fontSize / m_pAtlas->GetRenderSize();
    float ascent = m_pAtlas->GetAscent() * scale;
    float lineHeight = m_pAtlas->GetLineHeight() * scale;
    float penX = (float)p.posX;
    float startX = penX;
    float baselineY = (float)p.posY + ascent;

    for (int i = 0; wtext[i]; i++)
    {
        if (wtext[i] == L'\n')
        {
            penX = startX;
            baselineY += lineHeight;
            continue;
        }

        const SDFGlyph* g = m_pAtlas->GetGlyph(wtext[i]);
        if (!g || g->width == 0 || g->height == 0)
        {
            if (g) penX += g->advance * scale;
            continue;
        }

        float x0 = penX + g->xOffset * scale;
        float y0 = baselineY + g->yOffset * scale;
        float x1 = x0 + g->width * scale;
        float y1 = y0 + g->height * scale;

        verts.push_back({ x0, y0, g->u0, g->v0 });
        verts.push_back({ x1, y0, g->u1, g->v0 });
        verts.push_back({ x0, y1, g->u0, g->v1 });
        verts.push_back({ x1, y0, g->u1, g->v0 });
        verts.push_back({ x1, y1, g->u1, g->v1 });
        verts.push_back({ x0, y1, g->u0, g->v1 });

        penX += g->advance * scale;
    }
}

void SDFRenderer::Render()
{
    if (m_entries.empty() || !m_pCtx || !m_pAtlas || !m_pAtlas->GetTexture()) return;

    UINT stride = sizeof(QuadVertex);
    UINT offset = 0;
    float blendFactor[4] = { 0, 0, 0, 0 };
    ID3D11ShaderResourceView* atlas = m_pAtlas->GetTexture();

    m_pCtx->IASetInputLayout(m_pLayout);
    m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCtx->VSSetShader(m_pVS, nullptr, 0);
    m_pCtx->VSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetSamplers(0, 1, &m_pSampler);
    m_pCtx->OMSetBlendState(m_pAlphaBlend, blendFactor, 0xffffffff);
    m_pCtx->OMSetDepthStencilState(m_pDepthOff, 0);

    for (const auto& p : m_entries)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_pCtx->PSSetShaderResources(0, 1, &nullSRV);

        std::vector<QuadVertex> verts;
        BuildQuads(p, verts);
        if (verts.empty()) continue;
        if (!EnsureVertexCapacity(verts.size())) continue;

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(m_pCtx->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) continue;
        memcpy(mapped.pData, verts.data(), verts.size() * sizeof(QuadVertex));
        m_pCtx->Unmap(m_pVB, 0);

        SDFCB cb = {};
        cb.screenParams = D3DXVECTOR4((float)m_width, (float)m_height, 0, 0);
        cb.textColor = D3DXVECTOR4(p.color[0], p.color[1], p.color[2], p.color[3]);
        cb.outlineColor = D3DXVECTOR4(p.outlineColor[0], p.outlineColor[1], p.outlineColor[2], p.outlineColor[3]);
        cb.glowColor = D3DXVECTOR4(p.glowColor[0], p.glowColor[1], p.glowColor[2], p.glowColor[3]);
        cb.sdfParams = D3DXVECTOR4(p.outlineWidth, p.glowWidth, p.glowIntensity, 0);
        m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);

        int pass = p.effectMode;
        if (pass < 0 || pass >= SDF_EFFECT_COUNT) pass = 0;
        m_pCtx->PSSetShader(m_pPS[pass], nullptr, 0);
        m_pCtx->PSSetShaderResources(0, 1, &atlas);
        m_pCtx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
        m_pCtx->Draw((UINT)verts.size(), 0);
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pCtx->PSSetShaderResources(0, 1, &nullSRV);
    m_pCtx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pCtx->OMSetDepthStencilState(nullptr, 0);
}

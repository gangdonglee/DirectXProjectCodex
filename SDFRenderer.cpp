#include "SDFRenderer.h"
#include <cstring>

#define SDF_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

SDFRenderer::SDFRenderer()
    : m_pDev(nullptr), m_pAtlas(nullptr), m_pEffect(nullptr)
{
}

SDFRenderer::~SDFRenderer()
{
    Shutdown();
}

bool SDFRenderer::Init(IDirect3DDevice9* dev, SDFAtlas* atlas, const char* shaderPath)
{
    m_pDev   = dev;
    m_pAtlas = atlas;

    ID3DXBuffer* errors = nullptr;
    HRESULT hr = D3DXCreateEffectFromFileA(dev, shaderPath,
        nullptr, nullptr, 0, nullptr, &m_pEffect, &errors);
    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "SDF Shader Error", MB_OK);
            errors->Release();
        }
        return false;
    }
    return true;
}

void SDFRenderer::Shutdown()
{
    if (m_pEffect) { m_pEffect->Release(); m_pEffect = nullptr; }
    m_entries.clear();
}

SDFTextParams& SDFRenderer::Add(const char* text, int x, int y,
                                float fontSize, const float color[4])
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

void SDFRenderer::BuildQuads(const SDFTextParams& p, std::vector<QuadVertex>& verts)
{
    wchar_t wtext[1024];
    MultiByteToWideChar(CP_UTF8, 0, p.text, -1, wtext, 1024);

    float scale      = p.fontSize / m_pAtlas->GetRenderSize();
    float ascent     = m_pAtlas->GetAscent() * scale;
    float lineHeight = m_pAtlas->GetLineHeight() * scale;

    float penX      = (float)p.posX;
    float startX    = penX;
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

        float x0 = penX     + g->xOffset * scale;
        float y0 = baselineY + g->yOffset * scale;
        float x1 = x0 + g->width  * scale;
        float y1 = y0 + g->height * scale;

        // Triangle list: 2 triangles per glyph
        verts.push_back({ x0, y0, 0, 1, g->u0, g->v0 });
        verts.push_back({ x1, y0, 0, 1, g->u1, g->v0 });
        verts.push_back({ x0, y1, 0, 1, g->u0, g->v1 });

        verts.push_back({ x1, y0, 0, 1, g->u1, g->v0 });
        verts.push_back({ x1, y1, 0, 1, g->u1, g->v1 });
        verts.push_back({ x0, y1, 0, 1, g->u0, g->v1 });

        penX += g->advance * scale;
    }
}

void SDFRenderer::Render()
{
    if (m_entries.empty() || !m_pEffect || !m_pAtlas) return;

    for (auto& p : m_entries)
    {
        // Shader params
        D3DXVECTOR4 txtCol(p.color[0], p.color[1], p.color[2], p.color[3]);
        m_pEffect->SetVector("textColor", &txtCol);

        D3DXVECTOR4 outCol(p.outlineColor[0], p.outlineColor[1], p.outlineColor[2], p.outlineColor[3]);
        m_pEffect->SetVector("outlineColor", &outCol);
        m_pEffect->SetFloat("outlineWidth", p.outlineWidth);

        D3DXVECTOR4 glowCol(p.glowColor[0], p.glowColor[1], p.glowColor[2], p.glowColor[3]);
        m_pEffect->SetVector("glowColor", &glowCol);
        m_pEffect->SetFloat("glowWidth", p.glowWidth);
        m_pEffect->SetFloat("glowIntensity", p.glowIntensity);

        m_pEffect->SetTexture("SDFTexture", m_pAtlas->GetTexture());

        int pass = p.effectMode;
        if (pass >= SDF_EFFECT_COUNT) pass = 0;

        m_pEffect->SetTechnique("Tech_SDF");
        UINT passes = 0;
        m_pEffect->Begin(&passes, 0);
        m_pEffect->BeginPass(pass);

        std::vector<QuadVertex> verts;
        BuildQuads(p, verts);

        if (!verts.empty())
        {
            m_pDev->SetFVF(SDF_FVF);
            m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,
                (UINT)verts.size() / 3, verts.data(), sizeof(QuadVertex));
        }

        m_pEffect->EndPass();
        m_pEffect->End();
    }
}

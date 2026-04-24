#include "TextRenderer.h"
#include <algorithm>

#define QUAD_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

TextRenderer::TextRenderer()
    : m_pDev(nullptr), m_pEffect(nullptr),
      m_pRT(nullptr), m_pRTSurf(nullptr), m_pNoiseTex(nullptr),
      m_pStateBlock(nullptr),
      m_width(0), m_height(0)
{
}

TextRenderer::~TextRenderer()
{
    Shutdown();
}

D3DCOLOR TextRenderer::FloatToD3D(const float c[4])
{
    int r = (int)(c[0] * 255.0f + 0.5f);
    int g = (int)(c[1] * 255.0f + 0.5f);
    int b = (int)(c[2] * 255.0f + 0.5f);
    int a = (int)(c[3] * 255.0f + 0.5f);
    return D3DCOLOR_ARGB(a, r, g, b);
}

ID3DXFont* TextRenderer::GetOrCreateFont(int size)
{
    for (auto& f : m_fonts)
        if (f.size == size) return f.font;

    ID3DXFont* font = nullptr;
    if (SUCCEEDED(D3DXCreateFontW(m_pDev, size, 0, FW_BOLD, 1, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic", &font)))
    {
        m_fonts.push_back({ size, font });
        return font;
    }
    return nullptr;
}

bool TextRenderer::CreateNoiseTexture()
{
    HRESULT hr = D3DXCreateTextureFromFileA(m_pDev, "noise.png", &m_pNoiseTex);
    return SUCCEEDED(hr);
}

bool TextRenderer::Init(IDirect3DDevice9* dev, int width, int height, const char* shaderPath)
{
    m_pDev   = dev;
    m_width  = width;
    m_height = height;

    if (FAILED(dev->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pRT, nullptr)))
        return false;
    m_pRT->GetSurfaceLevel(0, &m_pRTSurf);

    ID3DXBuffer* errors = nullptr;
    HRESULT hr = D3DXCreateEffectFromFileA(dev, shaderPath,
        nullptr, nullptr, 0, nullptr, &m_pEffect, &errors);
    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Shader Error", MB_OK);
            errors->Release();
        }
        return false;
    }

    if (!CreateNoiseTexture())
        return false;

    return true;
}

void TextRenderer::Shutdown()
{
    if (m_pStateBlock) { m_pStateBlock->Release(); m_pStateBlock = nullptr; }
    if (m_pEffect)     { m_pEffect->Release();     m_pEffect     = nullptr; }
    if (m_pNoiseTex)   { m_pNoiseTex->Release();   m_pNoiseTex   = nullptr; }
    if (m_pRTSurf)     { m_pRTSurf->Release();     m_pRTSurf     = nullptr; }
    if (m_pRT)         { m_pRT->Release();          m_pRT         = nullptr; }
    for (auto& f : m_fonts)
        if (f.font) f.font->Release();
    m_fonts.clear();
    m_entries.clear();
}

// --- Multi-text API ---

TextParams& TextRenderer::Add(const char* text, int x, int y, int fontSize, const float color[4])
{
    TextParams p;
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

void TextRenderer::Remove(size_t index)
{
    if (index < m_entries.size())
        m_entries.erase(m_entries.begin() + index);
}

bool TextRenderer::Remove(const char* text, int x, int y, int fontSize, const float color[4])
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        if (it->Match(text, x, y, fontSize, color))
        {
            m_entries.erase(it);
            return true;
        }
    }
    return false;
}

void TextRenderer::Clear()
{
    m_entries.clear();
}

TextParams* TextRenderer::Find(const char* text, int x, int y, int fontSize, const float color[4])
{
    for (auto& e : m_entries)
        if (e.Match(text, x, y, fontSize, color))
            return &e;
    return nullptr;
}

std::vector<TextParams*> TextRenderer::FindAll(const char* text)
{
    std::vector<TextParams*> result;
    for (auto& e : m_entries)
        if (strcmp(e.text, text) == 0)
            result.push_back(&e);
    return result;
}

// --- Rendering ---

RECT TextRenderer::CalcTextRect(const TextParams& p, ID3DXFont* font)
{
    wchar_t wtext[1024];
    MultiByteToWideChar(CP_UTF8, 0, p.text, -1, wtext, 1024);

    RECT rc = { 0, 0, 0, 0 };
    font->DrawTextW(nullptr, wtext, -1, &rc, DT_CALCRECT, 0);

    int maxPad = (int)(p.fontSize * 0.4f);
    int pad = p.outlineEnabled ? (p.outlineSize < maxPad ? p.outlineSize : maxPad) : 0;
    RECT result;
    result.left   = p.posX - pad;
    result.top    = p.posY - pad;
    result.right  = p.posX + (rc.right - rc.left) + pad;
    result.bottom = p.posY + (rc.bottom - rc.top) + pad;
    return result;
}

void TextRenderer::RenderEntryToRT(const TextParams& p, ID3DXFont* font)
{
    IDirect3DSurface9* origRT = nullptr;
    IDirect3DSurface9* origDS = nullptr;
    m_pDev->GetRenderTarget(0, &origRT);
    m_pDev->GetDepthStencilSurface(&origDS);

    m_pDev->SetRenderTarget(0, m_pRTSurf);
    m_pDev->SetDepthStencilSurface(nullptr);
    m_pDev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    wchar_t wtext[1024];
    MultiByteToWideChar(CP_UTF8, 0, p.text, -1, wtext, 1024);

    // Draw white text only - shader handles coloring + outline
    RECT rc = { p.posX, p.posY, m_width, m_height };
    font->DrawTextW(nullptr, wtext, -1, &rc, DT_NOCLIP,
        D3DCOLOR_ARGB(255, 255, 255, 255));

    if (m_captureRT)
    {
        char path[256];
        sprintf_s(path, "rt_capture_%d.png", m_captureIdx++);
        D3DXSaveTextureToFileA(path, D3DXIFF_PNG, m_pRT, nullptr);
    }

    m_pDev->SetRenderTarget(0, origRT);
    m_pDev->SetDepthStencilSurface(origDS);
    if (origRT) origRT->Release();
    if (origDS) origDS->Release();
}

void TextRenderer::DrawEntryWithShader(const TextParams& p, const RECT& bounds)
{
    float x0 = (float)bounds.left;
    float y0 = (float)bounds.top;
    float x1 = (float)bounds.right;
    float y1 = (float)bounds.bottom;

    float u0 = x0 / (float)m_width;
    float v0 = y0 / (float)m_height;
    float u1 = x1 / (float)m_width;
    float v1 = y1 / (float)m_height;

    QuadVertex quad[4] =
    {
        { x0, y0, 0, 1,  u0, v0 },
        { x1, y0, 0, 1,  u1, v0 },
        { x0, y1, 0, 1,  u0, v1 },
        { x1, y1, 0, 1,  u1, v1 },
    };

    m_pEffect->SetTexture("SourceTex", m_pRT);

    D3DXVECTOR4 txtCol(p.color[0], p.color[1], p.color[2], p.color[3]);
    m_pEffect->SetVector("textColor", &txtCol);

    m_pEffect->SetFloat("texWidth",  (float)m_width);
    m_pEffect->SetFloat("texHeight", (float)m_height);

    // Set all uniforms that any pass might need
    D3DXVECTOR4 outColor(p.outlineColor[0], p.outlineColor[1], p.outlineColor[2], p.outlineColor[3]);
    m_pEffect->SetVector("outlineColor", &outColor);
    float maxOutline = p.fontSize * 0.4f;
    float effectiveOutline = (float)p.outlineSize < maxOutline ? (float)p.outlineSize : maxOutline;
    m_pEffect->SetFloat("outlineWidth", effectiveOutline);

    D3DXVECTOR4 glowCol(p.glowColor[0], p.glowColor[1], p.glowColor[2], p.glowColor[3]);
    m_pEffect->SetVector("glowColor", &glowCol);
    m_pEffect->SetFloat("glowWidth", p.glowWidth);
    m_pEffect->SetFloat("glowIntensity", p.glowIntensity);

    m_pEffect->SetTexture("NoiseTex", m_pNoiseTex);
    m_pEffect->SetFloat("progress", p.dissolveProgress);
    m_pEffect->SetFloat("edgeWidth", p.edgeWidth);
    float qMin[2] = { u0, v0 };
    float qMax[2] = { u1, v1 };
    m_pEffect->SetFloatArray("quadMin", qMin, 2);
    m_pEffect->SetFloatArray("quadMax", qMax, 2);
    D3DXVECTOR4 edgeCol(p.edgeColor[0], p.edgeColor[1], p.edgeColor[2], p.edgeColor[3]);
    m_pEffect->SetVector("edgeColor", &edgeCol);
    m_pEffect->SetFloat("edgeIntensity", p.edgeIntensity);

    // Tech_TextOutline: P0=Outline, P1=Text, P2=Glow, P3=OutlineGlow, P4=Dissolve, P5=OutlineDissolve
    int passIndex = 1; // default: P1_Text (Simple)
    switch (p.effectMode)
    {
    case EFFECT_SIMPLE:           passIndex = 1; break;
    case EFFECT_OUTLINE:          passIndex = 0; break;
    case EFFECT_GLOW:             passIndex = 2; break;
    case EFFECT_OUTLINE_GLOW:     passIndex = 3; break;
    case EFFECT_DISSOLVE:         passIndex = 4; break;
    case EFFECT_COMBINED:         passIndex = 5; break;
    }

    m_pEffect->SetTechnique("Tech_TextOutline");
    UINT passes = 0;
    m_pEffect->Begin(&passes, 0);
    m_pEffect->BeginPass(passIndex);
    m_pDev->SetFVF(QUAD_FVF);
    m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
    m_pEffect->EndPass();
    m_pEffect->End();
}

void TextRenderer::DrawDebugBorder(const RECT& r)
{
    struct LineVertex { float x, y, z, rhw; DWORD color; };
    const DWORD LINE_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    const DWORD RED = D3DCOLOR_ARGB(255, 255, 0, 0);

    float x0 = (float)r.left;
    float y0 = (float)r.top;
    float x1 = (float)r.right;
    float y1 = (float)r.bottom;

    LineVertex lines[5] =
    {
        { x0, y0, 0, 1, RED },
        { x1, y0, 0, 1, RED },
        { x1, y1, 0, 1, RED },
        { x0, y1, 0, 1, RED },
        { x0, y0, 0, 1, RED },
    };

    m_pDev->SetTexture(0, nullptr);
    m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDev->SetFVF(LINE_FVF);
    m_pDev->DrawPrimitiveUP(D3DPT_LINESTRIP, 4, lines, sizeof(LineVertex));
}

void TextRenderer::Render()
{
    if (m_entries.empty()) return;

    m_pDev->CreateStateBlock(D3DSBT_ALL, &m_pStateBlock);
    m_pStateBlock->Capture();

    m_pDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    for (auto& p : m_entries)
    {
        ID3DXFont* font = GetOrCreateFont(p.fontSize);
        if (!font) continue;

        RenderEntryToRT(p, font);

        RECT bounds = CalcTextRect(p, font);
        DrawEntryWithShader(p, bounds);

        if (m_debugBorder)
            DrawDebugBorder(bounds);
    }

    m_captureRT = false;

    m_pStateBlock->Apply();
    m_pStateBlock->Release();
    m_pStateBlock = nullptr;
}

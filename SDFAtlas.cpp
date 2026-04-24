#include "SDFAtlas.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

#include <cstdio>
#include <cstring>

SDFAtlas::SDFAtlas()
    : m_pDev(nullptr), m_pAtlasTex(nullptr), m_pFontInfo(nullptr),
      m_atlasSize(0), m_renderSize(0), m_fontScale(0),
      m_sdfPadding(0), m_packX(1), m_packY(1), m_rowHeight(0),
      m_ascent(0), m_descent(0), m_lineHeight(0)
{
}

SDFAtlas::~SDFAtlas()
{
    Shutdown();
}

bool SDFAtlas::LoadFont(const char* fontPath)
{
    FILE* f = fopen(fontPath, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    m_fontData.resize(size);
    fread(m_fontData.data(), 1, size, f);
    fclose(f);

    m_pFontInfo = new stbtt_fontinfo;
    if (!stbtt_InitFont(m_pFontInfo, m_fontData.data(), 0))
    {
        delete m_pFontInfo;
        m_pFontInfo = nullptr;
        return false;
    }
    return true;
}

bool SDFAtlas::CreateAtlasTexture()
{
    HRESULT hr = m_pDev->CreateTexture(
        m_atlasSize, m_atlasSize, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pAtlasTex, nullptr);
    if (FAILED(hr)) return false;

    D3DLOCKED_RECT lr;
    m_pAtlasTex->LockRect(0, &lr, nullptr, 0);
    memset(lr.pBits, 0, m_atlasSize * lr.Pitch);
    m_pAtlasTex->UnlockRect(0);
    return true;
}

bool SDFAtlas::Init(IDirect3DDevice9* dev, const char* fontPath,
                    float renderSize, int atlasSize, int sdfPadding)
{
    m_pDev       = dev;
    m_renderSize = renderSize;
    m_atlasSize  = atlasSize;
    m_sdfPadding = sdfPadding;
    m_packX      = 1;
    m_packY      = 1;
    m_rowHeight  = 0;

    if (!LoadFont(fontPath)) return false;

    m_fontScale = stbtt_ScaleForPixelHeight(m_pFontInfo, renderSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(m_pFontInfo, &ascent, &descent, &lineGap);
    m_ascent     = ascent * m_fontScale;
    m_descent    = descent * m_fontScale;
    m_lineHeight = (ascent - descent + lineGap) * m_fontScale;

    if (!CreateAtlasTexture()) return false;

    return true;
}

void SDFAtlas::Shutdown()
{
    if (m_pAtlasTex) { m_pAtlasTex->Release(); m_pAtlasTex = nullptr; }
    if (m_pFontInfo) { delete m_pFontInfo; m_pFontInfo = nullptr; }
    m_fontData.clear();
    m_glyphs.clear();
}

const SDFGlyph* SDFAtlas::GetGlyph(int codepoint)
{
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) return &it->second;

    if (RenderGlyphToAtlas(codepoint))
        return &m_glyphs[codepoint];
    return nullptr;
}

void SDFAtlas::PreloadChars(const wchar_t* chars)
{
    while (*chars)
    {
        GetGlyph(*chars);
        chars++;
    }
}

bool SDFAtlas::RenderGlyphToAtlas(int codepoint)
{
    if (!m_pFontInfo || !m_pAtlasTex) return false;

    int w, h, xoff, yoff;
    unsigned char* sdfData = stbtt_GetCodepointSDF(
        m_pFontInfo, m_fontScale, codepoint,
        m_sdfPadding, 128, 128.0f / m_sdfPadding,
        &w, &h, &xoff, &yoff);

    if (!sdfData)
    {
        // Space or empty glyph
        int advance, lsb;
        stbtt_GetCodepointHMetrics(m_pFontInfo, codepoint, &advance, &lsb);

        SDFGlyph g = {};
        g.advance = (int)(advance * m_fontScale);
        m_glyphs[codepoint] = g;
        return true;
    }

    // Row packing
    if (m_packX + w + 1 > m_atlasSize)
    {
        m_packX = 1;
        m_packY += m_rowHeight + 1;
        m_rowHeight = 0;
    }

    if (m_packY + h + 1 > m_atlasSize)
    {
        stbtt_FreeSDF(sdfData, nullptr);
        return false;
    }

    // Write SDF data to atlas texture
    D3DLOCKED_RECT lr;
    RECT lockRect = { m_packX, m_packY, m_packX + w, m_packY + h };
    if (SUCCEEDED(m_pAtlasTex->LockRect(0, &lr, &lockRect, 0)))
    {
        unsigned char* dst = (unsigned char*)lr.pBits;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                unsigned char v = sdfData[y * w + x];
                int idx = y * lr.Pitch + x * 4;
                dst[idx + 0] = v;   // B
                dst[idx + 1] = v;   // G
                dst[idx + 2] = v;   // R
                dst[idx + 3] = 255; // A
            }
        }
        m_pAtlasTex->UnlockRect(0);
    }

    int advance, lsb;
    stbtt_GetCodepointHMetrics(m_pFontInfo, codepoint, &advance, &lsb);

    SDFGlyph g;
    g.u0      = (float)m_packX / m_atlasSize;
    g.v0      = (float)m_packY / m_atlasSize;
    g.u1      = (float)(m_packX + w) / m_atlasSize;
    g.v1      = (float)(m_packY + h) / m_atlasSize;
    g.width   = w;
    g.height  = h;
    g.xOffset = xoff;
    g.yOffset = yoff;
    g.advance = (int)(advance * m_fontScale);
    m_glyphs[codepoint] = g;

    m_packX += w + 1;
    if (h > m_rowHeight) m_rowHeight = h;

    stbtt_FreeSDF(sdfData, nullptr);
    return true;
}

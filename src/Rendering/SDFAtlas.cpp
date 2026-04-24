#include "SDFAtlas.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

#include <cstdio>
#include <cstring>

SDFAtlas::SDFAtlas()
    : m_pDev(nullptr), m_pCtx(nullptr), m_pAtlasTex(nullptr), m_pAtlasSRV(nullptr), m_pFontInfo(nullptr),
      m_atlasSize(0), m_renderSize(0), m_fontScale(0), m_sdfPadding(0),
      m_packX(1), m_packY(1), m_rowHeight(0), m_ascent(0), m_descent(0), m_lineHeight(0)
{
}

SDFAtlas::~SDFAtlas()
{
    Shutdown();
}

bool SDFAtlas::LoadFont(const char* fontPath)
{
    FILE* f = nullptr;
    if (fopen_s(&f, fontPath, "rb") != 0 || !f) return false;
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
    m_pixels.assign(m_atlasSize * m_atlasSize * 4, 0);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = m_atlasSize;
    td.Height = m_atlasSize;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = m_pixels.data();
    init.SysMemPitch = m_atlasSize * 4;
    if (FAILED(m_pDev->CreateTexture2D(&td, &init, &m_pAtlasTex))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = td.Format;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    return SUCCEEDED(m_pDev->CreateShaderResourceView(m_pAtlasTex, &svd, &m_pAtlasSRV));
}

bool SDFAtlas::UploadAtlas()
{
    if (!m_pCtx || !m_pAtlasTex || m_pixels.empty()) return false;
    m_pCtx->UpdateSubresource(m_pAtlasTex, 0, nullptr, m_pixels.data(), m_atlasSize * 4, 0);
    return true;
}

bool SDFAtlas::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, const char* fontPath,
                    float renderSize, int atlasSize, int sdfPadding)
{
    m_pDev = dev;
    m_pCtx = ctx;
    m_renderSize = renderSize;
    m_atlasSize = atlasSize;
    m_sdfPadding = sdfPadding;
    m_packX = 1;
    m_packY = 1;
    m_rowHeight = 0;

    if (!LoadFont(fontPath)) return false;
    m_fontScale = stbtt_ScaleForPixelHeight(m_pFontInfo, renderSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(m_pFontInfo, &ascent, &descent, &lineGap);
    m_ascent = ascent * m_fontScale;
    m_descent = descent * m_fontScale;
    m_lineHeight = (ascent - descent + lineGap) * m_fontScale;

    return CreateAtlasTexture();
}

void SDFAtlas::Shutdown()
{
    if (m_pAtlasSRV) { m_pAtlasSRV->Release(); m_pAtlasSRV = nullptr; }
    if (m_pAtlasTex) { m_pAtlasTex->Release(); m_pAtlasTex = nullptr; }
    if (m_pFontInfo) { delete m_pFontInfo; m_pFontInfo = nullptr; }
    m_pixels.clear();
    m_fontData.clear();
    m_glyphs.clear();
}

const SDFGlyph* SDFAtlas::GetGlyph(int codepoint)
{
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) return &it->second;
    if (RenderGlyphToAtlas(codepoint)) return &m_glyphs[codepoint];
    return nullptr;
}

void SDFAtlas::PreloadChars(const wchar_t* chars)
{
    while (*chars)
    {
        GetGlyph(*chars);
        chars++;
    }
    UploadAtlas();
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
        int advance, lsb;
        stbtt_GetCodepointHMetrics(m_pFontInfo, codepoint, &advance, &lsb);
        SDFGlyph g = {};
        g.advance = (int)(advance * m_fontScale);
        m_glyphs[codepoint] = g;
        return true;
    }

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

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            unsigned char v = sdfData[y * w + x];
            int dst = ((m_packY + y) * m_atlasSize + (m_packX + x)) * 4;
            m_pixels[dst + 0] = 255;
            m_pixels[dst + 1] = 255;
            m_pixels[dst + 2] = 255;
            m_pixels[dst + 3] = v;
        }
    }

    int advance, lsb;
    stbtt_GetCodepointHMetrics(m_pFontInfo, codepoint, &advance, &lsb);

    SDFGlyph g;
    g.u0 = (float)m_packX / m_atlasSize;
    g.v0 = (float)m_packY / m_atlasSize;
    g.u1 = (float)(m_packX + w) / m_atlasSize;
    g.v1 = (float)(m_packY + h) / m_atlasSize;
    g.width = w;
    g.height = h;
    g.xOffset = xoff;
    g.yOffset = yoff;
    g.advance = (int)(advance * m_fontScale);
    m_glyphs[codepoint] = g;

    m_packX += w + 1;
    if (h > m_rowHeight) m_rowHeight = h;
    stbtt_FreeSDF(sdfData, nullptr);
    UploadAtlas();
    return true;
}

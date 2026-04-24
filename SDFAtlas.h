#pragma once
#include <d3d9.h>
#include <map>
#include <vector>

struct stbtt_fontinfo;

struct SDFGlyph
{
    float u0, v0, u1, v1;
    int   width, height;
    int   xOffset, yOffset;
    int   advance;
};

class SDFAtlas
{
public:
    SDFAtlas();
    ~SDFAtlas();

    bool Init(IDirect3DDevice9* dev, const char* fontPath,
              float renderSize = 48.0f, int atlasSize = 1024, int sdfPadding = 6);
    void Shutdown();

    const SDFGlyph* GetGlyph(int codepoint);
    void PreloadChars(const wchar_t* chars);

    IDirect3DTexture9* GetTexture() const { return m_pAtlasTex; }
    int   GetAtlasSize()  const { return m_atlasSize; }
    float GetRenderSize() const { return m_renderSize; }
    float GetAscent()     const { return m_ascent; }
    float GetDescent()    const { return m_descent; }
    float GetLineHeight() const { return m_lineHeight; }

private:
    bool LoadFont(const char* fontPath);
    bool CreateAtlasTexture();
    bool RenderGlyphToAtlas(int codepoint);

    IDirect3DDevice9*          m_pDev;
    IDirect3DTexture9*         m_pAtlasTex;
    std::vector<unsigned char> m_fontData;
    stbtt_fontinfo*            m_pFontInfo;
    std::map<int, SDFGlyph>    m_glyphs;

    int   m_atlasSize;
    float m_renderSize;
    float m_fontScale;
    int   m_sdfPadding;
    int   m_packX, m_packY, m_rowHeight;
    float m_ascent, m_descent, m_lineHeight;
};

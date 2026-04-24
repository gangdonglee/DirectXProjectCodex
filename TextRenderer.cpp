#include "TextRenderer.h"
#include <cstring>

TextRenderer::TextRenderer()
    : m_width(0), m_height(0), m_debugBorder(true), m_captureRT(false), m_captureIdx(0)
{
}

TextRenderer::~TextRenderer()
{
    Shutdown();
}

bool TextRenderer::Init(ID3D11Device* dev, int width, int height, const char* shaderPath)
{
    m_width = width;
    m_height = height;

    if (!dev) return false;
    dev->GetImmediateContext(&m_pCtx);
    if (!m_pCtx) return false;

    if (!m_atlas.Init(dev, m_pCtx, "C:\\Windows\\Fonts\\malgun.ttf", 48.0f, 1024, 6))
        return false;

    m_atlas.PreloadChars(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !?.,:-+/()[]{}<>_");

    return m_renderer.Init(dev, m_pCtx, &m_atlas, width, height, shaderPath);
}

void TextRenderer::Shutdown()
{
    m_renderer.Shutdown();
    m_atlas.Shutdown();
    if (m_pCtx)
    {
        m_pCtx->Release();
        m_pCtx = nullptr;
    }
    m_entries.clear();
}

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
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].Match(text, x, y, fontSize, color))
        {
            Remove(i);
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
    for (auto& p : m_entries)
        if (p.Match(text, x, y, fontSize, color))
            return &p;
    return nullptr;
}

std::vector<TextParams*> TextRenderer::FindAll(const char* text)
{
    std::vector<TextParams*> result;
    for (auto& p : m_entries)
        if (strcmp(p.text, text) == 0)
            result.push_back(&p);
    return result;
}

static int ToSDFEffect(const TextParams& p)
{
    switch (p.effectMode)
    {
    case EFFECT_OUTLINE:
        return SDF_OUTLINE;
    case EFFECT_GLOW:
        return SDF_GLOW;
    case EFFECT_OUTLINE_GLOW:
    case EFFECT_COMBINED:
        return SDF_OUTLINE_GLOW;
    case EFFECT_DISSOLVE:
    case EFFECT_SIMPLE:
        return p.outlineEnabled ? SDF_OUTLINE : SDF_SIMPLE;
    default:
        return p.outlineEnabled ? SDF_OUTLINE : SDF_SIMPLE;
    }
}

void TextRenderer::Render()
{
    m_renderer.Clear();
    for (const auto& p : m_entries)
    {
        SDFTextParams& sdf = m_renderer.Add(p.text, p.posX, p.posY, (float)p.fontSize, p.color);
        sdf.effectMode = ToSDFEffect(p);
        sdf.outlineColor[0] = p.outlineColor[0];
        sdf.outlineColor[1] = p.outlineColor[1];
        sdf.outlineColor[2] = p.outlineColor[2];
        sdf.outlineColor[3] = p.outlineColor[3];
        sdf.outlineWidth = (float)((p.outlineSize > 0) ? p.outlineSize : 1) / (float)((p.fontSize > 0) ? p.fontSize : 1);
        sdf.glowColor[0] = p.glowColor[0];
        sdf.glowColor[1] = p.glowColor[1];
        sdf.glowColor[2] = p.glowColor[2];
        sdf.glowColor[3] = p.glowColor[3];
        sdf.glowWidth = p.glowWidth / (float)((p.fontSize > 0) ? p.fontSize : 1);
        sdf.glowIntensity = p.glowIntensity;
    }
    m_renderer.Render();
    m_captureRT = false;
}

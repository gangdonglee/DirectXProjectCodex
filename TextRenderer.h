#pragma once
#include <d3d11.h>
#include <vector>
#include <cstring>
#include <cmath>
#include "SDFAtlas.h"
#include "SDFRenderer.h"

enum TextEffect
{
    EFFECT_SIMPLE = 0,
    EFFECT_OUTLINE,
    EFFECT_GLOW,
    EFFECT_OUTLINE_GLOW,
    EFFECT_DISSOLVE,
    EFFECT_COMBINED,
    EFFECT_COUNT
};

struct TextParams
{
    char  text[1024]     = "Hello, FontLib!";
    float color[4]       = { 1.0f, 1.0f, 0.0f, 1.0f };
    float outlineColor[4]= { 0.0f, 0.0f, 0.0f, 1.0f };
    int   posX           = 50;
    int   posY           = 50;
    int   fontSize       = 32;
    int   outlineSize    = 2;
    bool  outlineEnabled = true;

    // Effect mode
    int   effectMode     = EFFECT_OUTLINE;

    // Glow params
    float glowColor[4]     = { 0.4f, 0.7f, 1.0f, 1.0f };
    float glowWidth        = 5.0f;
    float glowIntensity    = 0.8f;

    // Dissolve params
    float dissolveProgress = 0.0f;
    float edgeWidth        = 0.1f;
    float noiseScale[2]    = { 1.0f, 1.0f };
    float edgeColor[4]     = { 1.0f, 0.8f, 0.2f, 1.0f };
    float edgeIntensity    = 1.5f;

    bool Match(const char* t, int x, int y, int size, const float c[4]) const
    {
        return strcmp(text, t) == 0
            && posX == x && posY == y
            && fontSize == size
            && fabsf(color[0]-c[0]) < 0.01f && fabsf(color[1]-c[1]) < 0.01f
            && fabsf(color[2]-c[2]) < 0.01f && fabsf(color[3]-c[3]) < 0.01f;
    }
};

class TextRenderer
{
public:
    TextRenderer();
    ~TextRenderer();

    bool Init(ID3D11Device* dev, int width, int height, const char* shaderPath);
    void Shutdown();
    void Render();

    // Multi-text management
    TextParams& Add(const char* text, int x = 50, int y = 50, int fontSize = 32,
                    const float color[4] = nullptr);
    void        Remove(size_t index);
    bool        Remove(const char* text, int x, int y, int fontSize, const float color[4]);
    void        Clear();

    TextParams*              Find(const char* text, int x, int y, int fontSize, const float color[4]);
    std::vector<TextParams*> FindAll(const char* text);
    TextParams&              Get(size_t index)       { return m_entries[index]; }
    size_t                   Count() const           { return m_entries.size(); }

    void SetDebugBorder(bool on) { m_debugBorder = on; }
    bool GetDebugBorder() const  { return m_debugBorder; }
    void CaptureRT()             { m_captureRT = true; }

private:
    std::vector<TextParams>  m_entries;

    ID3D11DeviceContext* m_pCtx = nullptr;
    SDFAtlas             m_atlas;
    SDFRenderer          m_renderer;
    int   m_width;
    int   m_height;
    bool  m_debugBorder = true;
    bool  m_captureRT  = false;
    int   m_captureIdx = 0;
};

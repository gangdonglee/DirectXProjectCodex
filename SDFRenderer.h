#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include "SDFAtlas.h"

enum SDFEffect
{
    SDF_SIMPLE = 0,
    SDF_OUTLINE,
    SDF_GLOW,
    SDF_OUTLINE_GLOW,
    SDF_EFFECT_COUNT
};

struct SDFTextParams
{
    char  text[1024]       = "SDF Hello!";
    float color[4]         = { 1, 1, 1, 1 };
    int   posX             = 50;
    int   posY             = 300;
    float fontSize         = 48.0f;
    int   effectMode       = SDF_SIMPLE;

    float outlineColor[4]  = { 0, 0, 0, 1 };
    float outlineWidth     = 0.1f;

    float glowColor[4]    = { 0.4f, 0.7f, 1.0f, 1.0f };
    float glowWidth        = 0.15f;
    float glowIntensity    = 0.8f;
};

class SDFRenderer
{
public:
    SDFRenderer();
    ~SDFRenderer();

    bool Init(IDirect3DDevice9* dev, SDFAtlas* atlas, const char* shaderPath);
    void Shutdown();
    void Render();

    SDFTextParams& Add(const char* text, int x = 50, int y = 300,
                       float fontSize = 48.0f, const float color[4] = nullptr);
    void           Remove(size_t index);
    void           Clear();
    SDFTextParams& Get(size_t index)       { return m_entries[index]; }
    size_t         Count() const           { return m_entries.size(); }

private:
    struct QuadVertex { float x, y, z, rhw, u, v; };

    void BuildQuads(const SDFTextParams& p, std::vector<QuadVertex>& verts);

    IDirect3DDevice9*           m_pDev;
    SDFAtlas*                   m_pAtlas;
    ID3DXEffect*                m_pEffect;
    std::vector<SDFTextParams>  m_entries;
};

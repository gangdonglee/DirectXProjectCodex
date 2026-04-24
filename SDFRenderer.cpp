#include "SDFRenderer.h"
#include "imgui.h"
#include <cstring>

SDFRenderer::SDFRenderer()
    : m_pDev(nullptr), m_pAtlas(nullptr)
{
}

SDFRenderer::~SDFRenderer()
{
    Shutdown();
}

bool SDFRenderer::Init(ID3D11Device* dev, SDFAtlas* atlas, const char*)
{
    m_pDev = dev;
    m_pAtlas = atlas;
    return true;
}

void SDFRenderer::Shutdown()
{
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

static ImU32 ToImColor(const float c[4])
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

void SDFRenderer::Render()
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    for (const auto& p : m_entries)
    {
        ImVec2 pos((float)p.posX, (float)p.posY);
        float size = p.fontSize;

        if (p.effectMode == SDF_OUTLINE || p.effectMode == SDF_OUTLINE_GLOW)
        {
            ImU32 outline = ToImColor(p.outlineColor);
            float o = 2.0f + p.outlineWidth * 12.0f;
            dl->AddText(nullptr, size, ImVec2(pos.x - o, pos.y), outline, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x + o, pos.y), outline, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y - o), outline, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y + o), outline, p.text);
        }

        if (p.effectMode == SDF_GLOW || p.effectMode == SDF_OUTLINE_GLOW)
        {
            ImVec4 glow(p.glowColor[0], p.glowColor[1], p.glowColor[2], p.glowColor[3] * 0.3f);
            ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glow);
            float g = 4.0f + p.glowWidth * 20.0f;
            dl->AddText(nullptr, size, ImVec2(pos.x - g, pos.y), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x + g, pos.y), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y - g), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y + g), glowCol, p.text);
        }

        dl->AddText(nullptr, size, pos, ToImColor(p.color), p.text);
    }
}

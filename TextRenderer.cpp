#include "TextRenderer.h"
#include "imgui.h"
#include <cstring>

TextRenderer::TextRenderer()
    : m_width(0), m_height(0), m_debugBorder(true), m_captureRT(false), m_captureIdx(0)
{
}

TextRenderer::~TextRenderer()
{
    Shutdown();
}

bool TextRenderer::Init(ID3D11Device*, int width, int height, const char*)
{
    m_width = width;
    m_height = height;
    return true;
}

void TextRenderer::Shutdown()
{
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

static ImU32 ToImColor(const float c[4])
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

void TextRenderer::Render()
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    for (const auto& p : m_entries)
    {
        ImVec2 pos((float)p.posX, (float)p.posY);
        float size = (float)p.fontSize;
        ImU32 col = ToImColor(p.color);

        bool outline = p.effectMode == EFFECT_OUTLINE || p.effectMode == EFFECT_OUTLINE_GLOW || p.effectMode == EFFECT_COMBINED || p.outlineEnabled;
        if (outline)
        {
            ImU32 outlineCol = ToImColor(p.outlineColor);
            float o = (float)((p.outlineSize > 0) ? p.outlineSize : 1);
            dl->AddText(nullptr, size, ImVec2(pos.x - o, pos.y), outlineCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x + o, pos.y), outlineCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y - o), outlineCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y + o), outlineCol, p.text);
        }

        if (p.effectMode == EFFECT_GLOW || p.effectMode == EFFECT_OUTLINE_GLOW || p.effectMode == EFFECT_COMBINED)
        {
            ImVec4 glow(p.glowColor[0], p.glowColor[1], p.glowColor[2], p.glowColor[3] * 0.35f);
            ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glow);
            float g = p.glowWidth;
            dl->AddText(nullptr, size, ImVec2(pos.x - g, pos.y), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x + g, pos.y), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y - g), glowCol, p.text);
            dl->AddText(nullptr, size, ImVec2(pos.x, pos.y + g), glowCol, p.text);
        }

        dl->AddText(nullptr, size, pos, col, p.text);

        if (m_debugBorder)
        {
            ImVec2 textSize = ImGui::CalcTextSize(p.text);
            float scale = size / ImGui::GetFontSize();
            textSize.x *= scale;
            textSize.y *= scale;
            dl->AddRect(pos, ImVec2(pos.x + textSize.x, pos.y + textSize.y), IM_COL32(255, 255, 0, 120));
        }
    }
    m_captureRT = false;
}

#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "DX9Device.h"
#include "TextRenderer.h"
#include "SDFAtlas.h"
#include "SDFRenderer.h"
#include "Map3D.h"
#include "UnitManager.h"

class App
{
public:
    App();
    ~App();

    bool Init(HINSTANCE hInst, int nCmdShow);
    int  Run();
    void Shutdown();

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    static const int WIDTH  = 1024;
    static const int HEIGHT = 720;

private:
    void InitImGui();
    void InitImGuiFonts();
    void RenderUI();
    void RenderSDFUI();
    void RenderMap3DUI();
    void RenderHealthBars();

    bool CreateTestBackground();
    void DrawBackground();

    HWND                m_hwnd;
    DX9Device           m_device;
    TextRenderer        m_textRenderer;
    SDFAtlas            m_sdfAtlas;
    SDFRenderer         m_sdfRenderer;
    Map3D               m_map3D;
    UnitManager         m_units;
    ID3DXEffect*        m_pHealthBarEffect;
    bool                m_showSDFAtlas;
    bool                m_enable3DMap;
    bool                m_showAllHealthBars;

    // Drag-box selection state
    bool                m_boxDragging;
    int                 m_boxStartX, m_boxStartY;
    int                 m_boxCurX,   m_boxCurY;
    IDirect3DTexture9*  m_pBgTex;
};

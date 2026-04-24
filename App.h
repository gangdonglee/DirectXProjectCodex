#pragma once
#include <windows.h>
#include "DX11Device.h"
#include "SDFAtlas.h"
#include "SDFRenderer.h"
#include "HealthBarRenderer.h"
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
    void RenderSDFUI();
    void RenderMap3DUI();
    void RenderHealthBars();

    void DrawBackground();

    HWND                m_hwnd;
    DX11Device          m_device;
    SDFAtlas            m_sdfAtlas;
    SDFRenderer         m_sdfRenderer;
    HealthBarRenderer   m_healthBarRenderer;
    Map3D               m_map3D;
    UnitManager         m_units;
    bool                m_showSDFAtlas;
    bool                m_enable3DMap;
    bool                m_showAllHealthBars;

    // Drag-box selection state
    bool                m_boxDragging;
    int                 m_boxStartX, m_boxStartY;
    int                 m_boxCurX,   m_boxCurY;
};

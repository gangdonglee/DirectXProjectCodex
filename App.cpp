#include "App.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <cstdio>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static App* s_pApp = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (s_pApp)
        return s_pApp->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

App::App()
    : m_hwnd(nullptr),
      m_showSDFAtlas(true), m_enable3DMap(true),
      m_showAllHealthBars(false),
      m_boxDragging(false),
      m_boxStartX(0), m_boxStartY(0), m_boxCurX(0), m_boxCurY(0)
{
    s_pApp = this;
}

App::~App()
{
    Shutdown();
    s_pApp = nullptr;
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Only call ImGui after context is created
    bool imguiReady = (ImGui::GetCurrentContext() != nullptr);
    if (imguiReady && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    bool imguiCapture  = imguiReady && ImGui::GetIO().WantCaptureMouse;
    bool imguiKeyboard = imguiReady && ImGui::GetIO().WantCaptureKeyboard;

    switch (msg)
    {
    case WM_DESTROY:    PostQuitMessage(0);                      return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        if (!imguiKeyboard) m_map3D.OnKeyDown((int)wp);
        return 0;
    case WM_KEYUP:
        m_map3D.OnKeyUp((int)wp);
        return 0;
    case WM_LBUTTONDOWN:
        if (!imguiCapture)
        {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            if (m_map3D.GetRTSMode())
            {
                // Begin potential drag-box
                m_boxDragging = true;
                m_boxStartX = m_boxCurX = mx;
                m_boxStartY = m_boxCurY = my;
                SetCapture(hwnd);
            }
            else
            {
                m_map3D.OnMouseDown(0, mx, my);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_boxDragging)
        {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            m_boxCurX = mx; m_boxCurY = my;
            int dx = abs(mx - m_boxStartX);
            int dy = abs(my - m_boxStartY);
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (dx < 4 && dy < 4)
            {
                // Single click: unit pick (player team only)
                D3DXVECTOR3 ro, rd;
                m_map3D.ScreenToRay(mx, my, &ro, &rd);
                int hit = m_units.PickUnit(ro, rd);
                if (!shift) m_units.ClearSelection();
                if (hit >= 0 && m_units.Get(hit).team == 0)
                    m_units.SetSelected(hit, true);
            }
            else
            {
                // Box select: player team only, alive, projected inside rect
                int x0 = m_boxStartX, x1 = mx; if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
                int y0 = m_boxStartY, y1 = my; if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

                if (!shift) m_units.ClearSelection();
                for (size_t i = 0; i < m_units.Count(); i++)
                {
                    const Unit& u = m_units.Get(i);
                    if (u.team != 0 || !u.alive) continue;
                    D3DXVECTOR3 center(u.position.x, u.position.y + 0.5f, u.position.z);
                    int sx, sy;
                    if (m_map3D.WorldToScreen(center, &sx, &sy))
                    {
                        if (sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1)
                            m_units.SetSelected((int)i, true);
                    }
                }
            }

            m_boxDragging = false;
            ReleaseCapture();
        }
        m_map3D.OnMouseUp(0, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_RBUTTONDOWN:
        if (!imguiCapture && m_map3D.GetRTSMode())
        {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);

            // First try picking an enemy unit -> attack command
            D3DXVECTOR3 ro, rd;
            m_map3D.ScreenToRay(mx, my, &ro, &rd);
            int hitIdx = m_units.PickUnit(ro, rd);

            if (hitIdx >= 0 && m_units.Get(hitIdx).team != 0)
            {
                m_units.AttackCommand(hitIdx);
                m_units.AddMarker(m_units.Get(hitIdx).position, 1.2f);
            }
            else
            {
                D3DXVECTOR3 ground;
                if (m_map3D.ScreenToGround(mx, my, &ground))
                {
                    m_units.MoveSelectedTo(ground);
                    m_units.AddMarker(ground, 1.2f);
                }
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if (m_boxDragging)
        {
            m_boxCurX = GET_X_LPARAM(lp);
            m_boxCurY = GET_Y_LPARAM(lp);
        }
        m_map3D.OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEWHEEL:
        if (!imguiCapture) m_map3D.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void App::DrawBackground()
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const int tile = 32;
    for (int y = 0; y < HEIGHT; y += tile)
    {
        for (int x = 0; x < WIDTH; x += tile)
        {
            bool light = ((x / tile) + (y / tile)) % 2 == 0;
            ImU32 col = light ? IM_COL32(80, 120, 200, 255) : IM_COL32(40, 60, 100, 255);
            dl->AddRectFilled(ImVec2((float)x, (float)y), ImVec2((float)(x + tile), (float)(y + tile)), col);
        }
    }
}

bool App::Init(HINSTANCE hInst, int nCmdShow)
{
    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = TEXT("DX11Window");
    if (!RegisterClassEx(&wc)) return false;

    RECT rc = { 0, 0, WIDTH, HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowEx(0, TEXT("DX11Window"), TEXT("DirectX Project - DX11 + ImGui"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    if (!m_hwnd) return false;

    if (!m_device.Init(m_hwnd, WIDTH, HEIGHT, true))
    {
        MessageBox(m_hwnd, TEXT("DX11 init failed"), TEXT("Error"), MB_OK);
        return false;
    }

    InitImGui();

    if (!m_textRenderer.Init(m_device.GetDevice(), WIDTH, HEIGHT, "shader.fx"))
    {
        MessageBox(m_hwnd, TEXT("Resource init failed"), TEXT("Error"), MB_OK);
        return false;
    }

    // Demo texts
    float yellow[] = { 1, 1, 0, 1 };
    float cyan[]   = { 0, 1, 1, 1 };
    float white[]  = { 1, 1, 1, 1 };
    m_textRenderer.Add("Hello, FontLib!", 50, 50, 32, yellow);
    m_textRenderer.Add("Multi-text!", 50, 120, 48, cyan);
    auto& sm = m_textRenderer.Add("Small text", 50, 200, 16, white);
    sm.outlineEnabled = false;
    sm.effectMode = EFFECT_SIMPLE;

    // SDF Font init
    if (!m_sdfAtlas.Init(m_device.GetDevice(), m_device.GetContext(), "C:\\Windows\\Fonts\\malgun.ttf", 48.0f, 1024, 6))
    {
        MessageBox(m_hwnd, TEXT("SDF Atlas init failed"), TEXT("Error"), MB_OK);
        return false;
    }
    m_sdfAtlas.PreloadChars(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !?.,");

    if (!m_sdfRenderer.Init(m_device.GetDevice(), m_device.GetContext(), &m_sdfAtlas, WIDTH, HEIGHT, "sdf.fx"))
    {
        MessageBox(m_hwnd, TEXT("SDF Renderer init failed"), TEXT("Error"), MB_OK);
        return false;
    }
    m_sdfRenderer.Add("SDF Hello!", 50, 350, 48.0f, yellow);

    if (!m_healthBarRenderer.Init(m_device.GetDevice(), m_device.GetContext(), WIDTH, HEIGHT, "healthbar.fx"))
    {
        MessageBox(m_hwnd, TEXT("HealthBar Renderer init failed"), TEXT("Error"), MB_OK);
        return false;
    }

    // 3D Map init
    if (!m_map3D.Init(m_device.GetDevice(), m_device.GetContext(), WIDTH, HEIGHT, 64, 1.0f))
    {
        MessageBox(m_hwnd, TEXT("Map3D init failed"), TEXT("Error"), MB_OK);
        return false;
    }

    // Units
    if (!m_units.Init(m_device.GetDevice(), m_device.GetContext()))
    {
        MessageBox(m_hwnd, TEXT("UnitManager init failed"), TEXT("Error"), MB_OK);
        return false;
    }
    // Player units (team 0, red)
    for (int z = 0; z < 3; z++)
        for (int x = 0; x < 4; x++)
            m_units.Add(D3DXVECTOR3(x * 2.0f - 3.0f, 0, z * 2.0f - 8.0f));

    // Enemy units (team 1, blue)
    for (int i = 0; i < 5; i++)
    {
        Unit& e = m_units.Add(D3DXVECTOR3(-4.0f + i * 2.0f, 0, 8.0f));
        e.team  = 1;
        e.hp    = 80;
        e.hpMax = 80;
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    return true;
}

void App::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device.GetDevice(), m_device.GetContext());
    InitImGuiFonts();
}

void App::InitImGuiFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    static const ImWchar thaiRange[] = { 0x0020, 0x00FF, 0x0E00, 0x0E7F, 0 };

    ImFontConfig cfg;
    cfg.SizePixels = 16.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, &cfg,
        io.Fonts->GetGlyphRangesKorean());

    cfg.MergeMode = true;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msjh.ttc", 16.0f, &cfg,
        io.Fonts->GetGlyphRangesChineseFull());

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\leelawui.ttf", 16.0f, &cfg,
        thaiRange);
}

void App::RenderUI()
{
    ImGui::Begin("Text Control");

    // Add new text
    static char newText[1024] = "";
    ImGui::InputText("New Text", newText, sizeof(newText));
    if (ImGui::Button("Add") && newText[0] != '\0')
    {
        float defColor[] = { 1, 1, 0, 1 };
        m_textRenderer.Add(newText, 50, 50 + (int)m_textRenderer.Count() * 60, 32, defColor);
        newText[0] = '\0';
    }

    ImGui::Separator();

    // Per-entry controls
    int removeIdx = -1;
    for (size_t i = 0; i < m_textRenderer.Count(); i++)
    {
        TextParams& p = m_textRenderer.Get(i);
        ImGui::PushID((int)i);

        char label[64];
        snprintf(label, sizeof(label), "[%d] %s", (int)i, p.text);
        if (ImGui::TreeNode(label))
        {
            ImGui::InputText("Text", p.text, sizeof(p.text));
            ImGui::SliderInt("X", &p.posX, 0, WIDTH);
            ImGui::SliderInt("Y", &p.posY, 0, HEIGHT);
            ImGui::ColorEdit4("Color", p.color);
            ImGui::SliderInt("Font Size", &p.fontSize, 8, 128);

            const char* effectNames[] = { "Simple", "Outline", "Glow", "Outline+Glow", "Dissolve", "Combined" };
            ImGui::Combo("Effect", &p.effectMode, effectNames, EFFECT_COUNT);

            if (p.effectMode == EFFECT_OUTLINE || p.effectMode == EFFECT_OUTLINE_GLOW || p.effectMode == EFFECT_COMBINED)
            {
                ImGui::SliderInt("Outline Size", &p.outlineSize, 1, 8);
                ImGui::ColorEdit4("Outline Color", p.outlineColor);
            }
            if (p.effectMode == EFFECT_GLOW || p.effectMode == EFFECT_OUTLINE_GLOW || p.effectMode == EFFECT_COMBINED)
            {
                ImGui::ColorEdit4("Glow Color", p.glowColor);
                ImGui::SliderFloat("Glow Width", &p.glowWidth, 1.0f, 10.0f);
                ImGui::SliderFloat("Glow Intensity", &p.glowIntensity, 0.1f, 3.0f);
            }
            if (p.effectMode == EFFECT_DISSOLVE || p.effectMode == EFFECT_COMBINED)
            {
                ImGui::SliderFloat("Progress", &p.dissolveProgress, 0.0f, 1.0f);
                ImGui::SliderFloat("Edge Width", &p.edgeWidth, 0.01f, 0.5f);
                ImGui::ColorEdit4("Edge Color", p.edgeColor);
                ImGui::SliderFloat("Edge Intensity", &p.edgeIntensity, 0.1f, 5.0f);
            }
            if (ImGui::Button("Remove"))
                removeIdx = (int)i;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (removeIdx >= 0)
        m_textRenderer.Remove((size_t)removeIdx);

    ImGui::Separator();
    bool dbg = m_textRenderer.GetDebugBorder();
    if (ImGui::Checkbox("Debug Border", &dbg))
        m_textRenderer.SetDebugBorder(dbg);

    if (ImGui::Button("Capture RT"))
        m_textRenderer.CaptureRT();

    ImGui::Text("Total: %d", (int)m_textRenderer.Count());
    ImGui::End();
}

void App::RenderSDFUI()
{
    ImGui::Begin("SDF Font");

    // Add new SDF text
    static char newSDFText[1024] = "";
    ImGui::InputText("New SDF Text", newSDFText, sizeof(newSDFText));
    if (ImGui::Button("Add SDF") && newSDFText[0] != '\0')
    {
        float defColor[] = { 1, 1, 1, 1 };
        m_sdfRenderer.Add(newSDFText, 50, 350 + (int)m_sdfRenderer.Count() * 60, 48.0f, defColor);
        newSDFText[0] = '\0';
    }

    ImGui::Separator();

    // Per-entry controls
    int removeIdx = -1;
    for (size_t i = 0; i < m_sdfRenderer.Count(); i++)
    {
        SDFTextParams& p = m_sdfRenderer.Get(i);
        ImGui::PushID((int)(1000 + i));

        char label[64];
        snprintf(label, sizeof(label), "[SDF %d] %s", (int)i, p.text);
        if (ImGui::TreeNode(label))
        {
            ImGui::InputText("Text", p.text, sizeof(p.text));
            ImGui::SliderInt("X", &p.posX, 0, WIDTH);
            ImGui::SliderInt("Y", &p.posY, 0, HEIGHT);
            ImGui::SliderFloat("Font Size", &p.fontSize, 8.0f, 200.0f);
            ImGui::ColorEdit4("Color", p.color);

            const char* effectNames[] = { "Simple", "Outline", "Glow", "Outline+Glow" };
            ImGui::Combo("Effect", &p.effectMode, effectNames, SDF_EFFECT_COUNT);

            if (p.effectMode == SDF_OUTLINE || p.effectMode == SDF_OUTLINE_GLOW)
            {
                ImGui::SliderFloat("Outline Width", &p.outlineWidth, 0.01f, 0.3f);
                ImGui::ColorEdit4("Outline Color", p.outlineColor);
            }
            if (p.effectMode == SDF_GLOW || p.effectMode == SDF_OUTLINE_GLOW)
            {
                ImGui::ColorEdit4("Glow Color", p.glowColor);
                ImGui::SliderFloat("Glow Width", &p.glowWidth, 0.01f, 0.4f);
                ImGui::SliderFloat("Glow Intensity", &p.glowIntensity, 0.1f, 3.0f);
            }

            if (ImGui::Button("Remove"))
                removeIdx = (int)i;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (removeIdx >= 0)
        m_sdfRenderer.Remove((size_t)removeIdx);

    ImGui::Separator();
    ImGui::Checkbox("Show Atlas", &m_showSDFAtlas);
    ImGui::Text("Glyphs cached: atlas %dx%d", m_sdfAtlas.GetAtlasSize(), m_sdfAtlas.GetAtlasSize());
    ImGui::End();

    // Debug atlas window
    if (m_showSDFAtlas && m_sdfAtlas.GetTexture())
    {
        ImGui::Begin("SDF Atlas Debug", &m_showSDFAtlas);
        float viewSize = 512.0f;
        ImGui::Image((ImTextureID)(intptr_t)m_sdfAtlas.GetTexture(), ImVec2(viewSize, viewSize));
        ImGui::End();
    }
}

void App::RenderMap3DUI()
{
    ImGui::Begin("3D Map");

    ImGui::Checkbox("Enable 3D Map", &m_enable3DMap);

    if (m_enable3DMap)
    {
        ImGui::Text("Grid size: %dx%d", m_map3D.GetGridSize(), m_map3D.GetGridSize());

        bool rts = m_map3D.GetRTSMode();
        if (ImGui::Checkbox("RTS Mode", &rts))
            m_map3D.SetRTSMode(rts);

        if (rts)
        {
            float rtsPitchDeg = m_map3D.GetRTSPitch() * 57.2958f;
            if (ImGui::SliderFloat("RTS Pitch", &rtsPitchDeg, 20.0f, 85.0f))
                m_map3D.SetRTSPitch(rtsPitchDeg / 57.2958f);
        }

        D3DXVECTOR3 tgt = m_map3D.GetTarget();
        float target[3] = { tgt.x, tgt.y, tgt.z };
        if (ImGui::DragFloat3("Target", target, 0.2f))
            m_map3D.SetTarget(D3DXVECTOR3(target[0], target[1], target[2]));

        float dist = m_map3D.GetDistance();
        if (ImGui::SliderFloat("Distance", &dist, 1.0f, 300.0f))
            m_map3D.SetDistance(dist);

        float yawDeg = m_map3D.GetYaw() * 57.2958f;
        if (ImGui::SliderFloat("Yaw", &yawDeg, -360.0f, 360.0f))
            m_map3D.SetYaw(yawDeg / 57.2958f);

        float pitchDeg = m_map3D.GetPitch() * 57.2958f;
        if (ImGui::SliderFloat("Pitch", &pitchDeg, -89.0f, 89.0f))
            m_map3D.SetPitch(pitchDeg / 57.2958f);

        float fovDeg = m_map3D.GetFOV() * 57.2958f;
        if (ImGui::SliderFloat("FOV", &fovDeg, 20.0f, 120.0f))
            m_map3D.SetFOV(fovDeg / 57.2958f);

        bool wire = m_map3D.GetShowWireframe();
        if (ImGui::Checkbox("Wireframe", &wire))
            m_map3D.SetShowWireframe(wire);

        ImGui::Separator();
        bool fog = m_map3D.GetFogEnabled();
        if (ImGui::Checkbox("Distance Fog", &fog))
            m_map3D.SetFogEnabled(fog);
        if (fog)
        {
            float fs = m_map3D.GetFogStart();
            if (ImGui::SliderFloat("Fog Start", &fs, 1.0f, 200.0f))
                m_map3D.SetFogStart(fs);
            float fe = m_map3D.GetFogEnd();
            if (ImGui::SliderFloat("Fog End", &fe, 1.0f, 300.0f))
                m_map3D.SetFogEnd(fe);

            DWORD c = m_map3D.GetFogColor();
            float col[3] = {
                ((c >> 16) & 0xFF) / 255.0f,
                ((c >> 8)  & 0xFF) / 255.0f,
                ( c        & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit3("Fog Color", col))
            {
                BYTE r = (BYTE)(col[0] * 255);
                BYTE g = (BYTE)(col[1] * 255);
                BYTE b = (BYTE)(col[2] * 255);
                m_map3D.SetFogColor(D3DCOLOR_ARGB(0, r, g, b));
            }
        }

        float moveSpeed = m_map3D.GetMoveSpeed();
        if (ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f))
            m_map3D.SetMoveSpeed(moveSpeed);

        ImGui::Separator();
        if (m_map3D.GetRTSMode())
        {
            ImGui::TextDisabled("WASD / edge-scroll: pan");
            ImGui::TextDisabled("Q/E: rotate yaw | Wheel: zoom");
        }
        else
        {
            ImGui::TextDisabled("Left-drag: rotate | Wheel: zoom");
            ImGui::TextDisabled("WASD: pan | Q/E: down/up");
        }

        // Picking test: mouse -> world ground point
        POINT cp;
        if (GetCursorPos(&cp) && ScreenToClient(m_hwnd, &cp))
        {
            D3DXVECTOR3 hit;
            if (m_map3D.ScreenToGround(cp.x, cp.y, &hit))
                ImGui::Text("Ground @ (%.2f, %.2f, %.2f)", hit.x, hit.y, hit.z);
            else
                ImGui::Text("Ground: (no hit)");
        }

        ImGui::Separator();
        ImGui::Text("Units: %d (selected: %d)",
            (int)m_units.Count(), m_units.SelectedCount());
        ImGui::Checkbox("Show all HP bars", &m_showAllHealthBars);

        if (ImGui::Button("Damage Selected (-15)"))
        {
            for (size_t i = 0; i < m_units.Count(); i++)
            {
                Unit& u = m_units.Get(i);
                if (u.selected)
                {
                    u.hp = (u.hp > 15) ? u.hp - 15 : 0;
                    u.hitFlashTimer = 0.15f;
                    if (u.hp <= 0) u.alive = false;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Heal All"))
        {
            for (size_t i = 0; i < m_units.Count(); i++)
                m_units.Get(i).hp = m_units.Get(i).hpMax;
        }
        if (ImGui::Button("Respawn Enemies"))
        {
            for (size_t i = 0; i < m_units.Count(); i++)
            {
                Unit& u = m_units.Get(i);
                if (u.team == 1)
                {
                    u.alive = true;
                    u.hp    = u.hpMax;
                    u.attackTarget = -1;
                }
            }
        }

        ImGui::TextDisabled("Left-click: select | Drag: box-select");
        ImGui::TextDisabled("Right-click ground: move | enemy: attack");
    }

    ImGui::End();
}

void App::RenderHealthBars()
{
    if (!m_enable3DMap) return;

    const float barW = 40.0f;
    const float barH = 5.0f;
    for (size_t i = 0; i < m_units.Count(); i++)
    {
        const Unit& u = m_units.Get(i);
        if (!u.alive) continue;
        float ratio = (u.hpMax > 0) ? (u.hp / u.hpMax) : 0;
        bool damaged = ratio < 1.0f;

        if (!m_showAllHealthBars && !u.selected && !damaged) continue;

        D3DXVECTOR3 top(u.position.x, u.position.y + 1.25f, u.position.z);
        int sx, sy;
        if (!m_map3D.WorldToScreen(top, &sx, &sy)) continue;

        float cx = (float)sx, cy = (float)sy;
        float x0 = cx - barW * 0.5f;
        float x1 = cx + barW * 0.5f;
        float y0 = cy - barH * 0.5f;
        float y1 = cy + barH * 0.5f;

        m_healthBarRenderer.Render(x0, y0, x1, y1, ratio);
    }
}

int App::Run()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();
        RenderSDFUI();
        RenderMap3DUI();

        // Drag-box overlay (drawn on top of everything in ImGui layer)
        if (m_boxDragging)
        {
            int x0 = m_boxStartX, x1 = m_boxCurX; if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            int y0 = m_boxStartY, y1 = m_boxCurY; if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
            if ((x1 - x0 > 2) || (y1 - y0 > 2))
            {
                ImDrawList* dl = ImGui::GetForegroundDrawList();
                ImU32 fill = IM_COL32(100, 200, 100, 40);
                ImU32 line = IM_COL32(120, 255, 120, 200);
                dl->AddRectFilled(ImVec2((float)x0, (float)y0), ImVec2((float)x1, (float)y1), fill);
                dl->AddRect      (ImVec2((float)x0, (float)y0), ImVec2((float)x1, (float)y1), line, 0, 0, 1.5f);
            }
        }

        // Frame timing (simple delta)
        static DWORD lastTick = GetTickCount();
        DWORD nowTick = GetTickCount();
        float dt = (nowTick - lastTick) / 1000.0f;
        lastTick = nowTick;

        // Edge-scroll using current mouse position (RTS mode only)
        POINT cp;
        if (GetCursorPos(&cp) && ScreenToClient(m_hwnd, &cp))
            m_map3D.UpdateEdgeScroll(cp.x, cp.y, dt);

        m_map3D.Update(dt);
        m_units.Update(dt);

        m_device.BeginFrame();

        if (m_enable3DMap)
        {
            m_map3D.Render();
            m_units.Render(m_map3D);
        }
        else
            DrawBackground();

        RenderHealthBars();
        m_textRenderer.Render();
        m_sdfRenderer.Render();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        m_device.EndFrame();
        m_device.Present();
    }
    return (int)msg.wParam;
}

void App::Shutdown()
{
    m_units.Shutdown();
    m_map3D.Shutdown();
    m_healthBarRenderer.Shutdown();
    m_sdfRenderer.Shutdown();
    m_sdfAtlas.Shutdown();
    m_textRenderer.Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_device.Shutdown();
}

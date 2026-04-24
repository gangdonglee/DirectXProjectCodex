#pragma once
#include <d3d9.h>
#include <d3dx9.h>

class Map3D
{
public:
    Map3D();
    ~Map3D();

    bool Init(IDirect3DDevice9* dev, int width, int height,
              int gridSize = 64, float tileSize = 1.0f,
              const char* terrainShaderPath = "terrain.fx");
    void Shutdown();

    void Update(float dt);
    void Render();

    // Mouse/keyboard input
    void OnMouseDown(int button, int x, int y);
    void OnMouseUp(int button, int x, int y);
    void OnMouseMove(int x, int y);
    void OnMouseWheel(int delta);
    void OnKeyDown(int vkey);
    void OnKeyUp(int vkey);

    void SetMoveSpeed(float s) { m_moveSpeed = s; }
    float GetMoveSpeed() const { return m_moveSpeed; }

    // RTS mode
    void SetRTSMode(bool on)       { m_rtsMode = on; }
    bool GetRTSMode() const        { return m_rtsMode; }
    void SetRTSPitch(float p)      { m_rtsPitch = p; }
    float GetRTSPitch() const      { return m_rtsPitch; }

    // Screen size (for edge-scroll etc.)
    void SetScreenSize(int w, int h) { m_screenWidth = w; m_screenHeight = h; }
    int  GetScreenWidth() const      { return m_screenWidth; }
    int  GetScreenHeight() const     { return m_screenHeight; }

    // For edge-scroll: called each frame with current mouse position
    void UpdateEdgeScroll(int mouseX, int mouseY, float dt);

    // Picking: screen -> world ray / ground point (y=0)
    void ScreenToRay(int sx, int sy, D3DXVECTOR3* origin, D3DXVECTOR3* dir) const;
    bool ScreenToGround(int sx, int sy, D3DXVECTOR3* outHit) const;
    // World -> screen (returns false if behind camera)
    bool WorldToScreen(const D3DXVECTOR3& world, int* sx, int* sy) const;

    const D3DXMATRIX& GetViewMatrix() const { return m_matView; }
    const D3DXMATRIX& GetProjMatrix() const { return m_matProj; }
    const D3DXVECTOR3& GetEyePos()    const { return m_eyePos; }

    // Helper: set common fog + eyePos uniforms on any effect that uses them
    void SetFogUniforms(ID3DXEffect* effect) const;

    // Camera controls
    void   SetTarget(const D3DXVECTOR3& t) { m_target = t; }
    void   SetDistance(float d)            { m_distance = d; }
    void   SetYaw(float y)                 { m_yaw = y; }
    void   SetPitch(float p)               { m_pitch = p; }
    void   SetFOV(float fov)               { m_fov = fov; }

    D3DXVECTOR3 GetTarget()   const { return m_target; }
    float       GetDistance() const { return m_distance; }
    float       GetYaw()      const { return m_yaw; }
    float       GetPitch()    const { return m_pitch; }
    float       GetFOV()      const { return m_fov; }
    int         GetGridSize() const { return m_gridSize; }

    bool  GetShowWireframe() const { return m_wireframe; }
    void  SetShowWireframe(bool b) { m_wireframe = b; }

    // Distance fog
    bool  GetFogEnabled()  const   { return m_fogEnabled; }
    void  SetFogEnabled(bool b)    { m_fogEnabled = b; }
    float GetFogStart()    const   { return m_fogStart; }
    void  SetFogStart(float v)     { m_fogStart = v; }
    float GetFogEnd()      const   { return m_fogEnd; }
    void  SetFogEnd(float v)       { m_fogEnd = v; }
    DWORD GetFogColor()    const   { return m_fogColor; }
    void  SetFogColor(DWORD c)     { m_fogColor = c; }

private:
    struct MapVertex
    {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    bool CreateGridMesh();
    bool CreateCheckerTexture();
    void UpdateMatrices();

    IDirect3DDevice9*       m_pDev;
    IDirect3DVertexBuffer9* m_pVB;
    IDirect3DIndexBuffer9*  m_pIB;
    IDirect3DTexture9*      m_pCheckerTex;
    ID3DXEffect*            m_pTerrainEffect;

    int   m_gridSize;
    float m_tileSize;
    int   m_vertexCount;
    int   m_indexCount;

    int   m_screenWidth;
    int   m_screenHeight;

    // Orbit camera
    D3DXVECTOR3 m_target;
    float       m_distance;
    float       m_yaw;     // horizontal angle (radians)
    float       m_pitch;   // vertical angle (radians)
    float       m_fov;
    float       m_nearZ;
    float       m_farZ;

    // Mouse state
    bool m_dragging;
    int  m_lastMouseX;
    int  m_lastMouseY;

    // WASD key state
    bool  m_keyW, m_keyA, m_keyS, m_keyD, m_keyQ, m_keyE;
    float m_moveSpeed;

    // RTS mode
    bool  m_rtsMode;
    float m_rtsPitch;        // fixed pitch in RTS mode
    float m_edgeScrollMargin; // px from edge that triggers scroll

    // Render state
    bool m_wireframe;

    // Distance fog
    bool  m_fogEnabled;
    DWORD m_fogColor;
    float m_fogStart;
    float m_fogEnd;

    // Cached matrices
    D3DXMATRIX  m_matView;
    D3DXMATRIX  m_matProj;
    D3DXMATRIX  m_matWorld;
    D3DXVECTOR3 m_eyePos;
};

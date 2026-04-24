#include "Map3D.h"
#include <vector>
#include <cmath>

#define MAP_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

Map3D::Map3D()
    : m_pDev(nullptr), m_pVB(nullptr), m_pIB(nullptr),
      m_pCheckerTex(nullptr), m_pTerrainEffect(nullptr),
      m_gridSize(0), m_tileSize(0), m_vertexCount(0), m_indexCount(0),
      m_screenWidth(0), m_screenHeight(0),
      m_target(0, 0, 0), m_distance(50.0f),
      m_yaw(0.7f), m_pitch(0.6f),
      m_fov(D3DX_PI / 4.0f), m_nearZ(0.1f), m_farZ(500.0f),
      m_dragging(false), m_lastMouseX(0), m_lastMouseY(0),
      m_keyW(false), m_keyA(false), m_keyS(false), m_keyD(false),
      m_keyQ(false), m_keyE(false), m_moveSpeed(20.0f),
      m_rtsMode(true), m_rtsPitch(D3DX_PI * 0.35f),   // ~63 deg
      m_edgeScrollMargin(12.0f),
      m_wireframe(false),
      m_fogEnabled(true),
      m_fogColor(D3DCOLOR_ARGB(0, 95, 120, 150)),
      m_fogStart(30.0f), m_fogEnd(90.0f)
{
    D3DXMatrixIdentity(&m_matView);
    D3DXMatrixIdentity(&m_matProj);
    D3DXMatrixIdentity(&m_matWorld);
}

Map3D::~Map3D()
{
    Shutdown();
}

bool Map3D::Init(IDirect3DDevice9* dev, int width, int height, int gridSize, float tileSize,
                 const char* terrainShaderPath)
{
    m_pDev         = dev;
    m_screenWidth  = width;
    m_screenHeight = height;
    m_gridSize     = gridSize;
    m_tileSize     = tileSize;

    m_target = D3DXVECTOR3(0, 0, 0);
    m_distance = gridSize * tileSize * 1.2f;

    if (!CreateGridMesh())       return false;
    if (!CreateCheckerTexture()) return false;

    ID3DXBuffer* err = nullptr;
    HRESULT hr = D3DXCreateEffectFromFileA(dev, terrainShaderPath,
        nullptr, nullptr, 0, nullptr, &m_pTerrainEffect, &err);
    if (FAILED(hr))
    {
        if (err)
        {
            MessageBoxA(nullptr, (char*)err->GetBufferPointer(), "Terrain Shader Error", MB_OK);
            err->Release();
        }
        return false;
    }

    UpdateMatrices();
    return true;
}

void Map3D::Shutdown()
{
    if (m_pTerrainEffect) { m_pTerrainEffect->Release(); m_pTerrainEffect = nullptr; }
    if (m_pCheckerTex)    { m_pCheckerTex->Release();    m_pCheckerTex    = nullptr; }
    if (m_pIB)            { m_pIB->Release();            m_pIB            = nullptr; }
    if (m_pVB)            { m_pVB->Release();            m_pVB            = nullptr; }
}

bool Map3D::CreateGridMesh()
{
    int verticesPerSide = m_gridSize + 1;
    m_vertexCount = verticesPerSide * verticesPerSide;
    m_indexCount  = m_gridSize * m_gridSize * 6;

    float half = (m_gridSize * m_tileSize) * 0.5f;

    // Build vertices
    std::vector<MapVertex> verts(m_vertexCount);
    for (int z = 0; z < verticesPerSide; z++)
    {
        for (int x = 0; x < verticesPerSide; x++)
        {
            MapVertex& v = verts[z * verticesPerSide + x];
            v.x = x * m_tileSize - half;
            v.y = 0.0f;                         // flat grid (heightmap extension later)
            v.z = z * m_tileSize - half;
            v.nx = 0; v.ny = 1; v.nz = 0;       // up normal
            v.u = (float)x;                      // tile UV (1 repetition per quad)
            v.v = (float)z;
        }
    }

    // Build indices
    std::vector<DWORD> indices(m_indexCount);
    int idx = 0;
    for (int z = 0; z < m_gridSize; z++)
    {
        for (int x = 0; x < m_gridSize; x++)
        {
            int topLeft     =  z      * verticesPerSide + x;
            int topRight    =  z      * verticesPerSide + (x + 1);
            int bottomLeft  = (z + 1) * verticesPerSide + x;
            int bottomRight = (z + 1) * verticesPerSide + (x + 1);

            indices[idx++] = topLeft;
            indices[idx++] = bottomLeft;
            indices[idx++] = topRight;

            indices[idx++] = topRight;
            indices[idx++] = bottomLeft;
            indices[idx++] = bottomRight;
        }
    }

    // Create VB
    UINT vbSize = m_vertexCount * sizeof(MapVertex);
    if (FAILED(m_pDev->CreateVertexBuffer(vbSize, 0, MAP_FVF, D3DPOOL_MANAGED, &m_pVB, nullptr)))
        return false;

    void* pVB = nullptr;
    m_pVB->Lock(0, 0, &pVB, 0);
    memcpy(pVB, verts.data(), vbSize);
    m_pVB->Unlock();

    // Create IB
    UINT ibSize = m_indexCount * sizeof(DWORD);
    if (FAILED(m_pDev->CreateIndexBuffer(ibSize, 0, D3DFMT_INDEX32, D3DPOOL_MANAGED, &m_pIB, nullptr)))
        return false;

    void* pIB = nullptr;
    m_pIB->Lock(0, 0, &pIB, 0);
    memcpy(pIB, indices.data(), ibSize);
    m_pIB->Unlock();

    return true;
}

bool Map3D::CreateCheckerTexture()
{
    const int TEX_SIZE = 64;
    if (FAILED(m_pDev->CreateTexture(TEX_SIZE, TEX_SIZE, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pCheckerTex, nullptr)))
        return false;

    D3DLOCKED_RECT lr;
    m_pCheckerTex->LockRect(0, &lr, nullptr, 0);
    DWORD* pixels = (DWORD*)lr.pBits;
    int pitch = lr.Pitch / 4;

    for (int y = 0; y < TEX_SIZE; y++)
    {
        for (int x = 0; x < TEX_SIZE; x++)
        {
            // Border lines for grid feel
            bool isBorder = (x < 2) || (x >= TEX_SIZE - 2) || (y < 2) || (y >= TEX_SIZE - 2);
            if (isBorder)
                pixels[y * pitch + x] = D3DCOLOR_ARGB(255, 50, 50, 50);
            else
                pixels[y * pitch + x] = D3DCOLOR_ARGB(255, 180, 200, 220);
        }
    }
    m_pCheckerTex->UnlockRect(0);
    return true;
}

void Map3D::UpdateMatrices()
{
    // Clamp pitch to avoid gimbal issues
    const float maxPitch = D3DX_PI * 0.49f;
    if (m_pitch > maxPitch)  m_pitch = maxPitch;
    if (m_pitch < -maxPitch) m_pitch = -maxPitch;

    // Orbit camera position
    float cosP = cosf(m_pitch);
    D3DXVECTOR3 offset(
        m_distance * cosP * sinf(m_yaw),
        m_distance * sinf(m_pitch),
        m_distance * cosP * cosf(m_yaw)
    );
    D3DXVECTOR3 eye = m_target + offset;
    m_eyePos = eye;
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtLH(&m_matView, &eye, &m_target, &up);

    float aspect = (float)m_screenWidth / (float)m_screenHeight;
    D3DXMatrixPerspectiveFovLH(&m_matProj, m_fov, aspect, m_nearZ, m_farZ);

    D3DXMatrixIdentity(&m_matWorld);
}

void Map3D::Update(float dt)
{
    float moveDist = m_moveSpeed * dt;

    D3DXVECTOR3 forward(-sinf(m_yaw), 0, -cosf(m_yaw));
    D3DXVECTOR3 right(-cosf(m_yaw), 0, sinf(m_yaw));

    if (m_rtsMode)
    {
        // RTS: fixed pitch, Q/E yaw rotation
        m_pitch = m_rtsPitch;

        if (m_keyW) m_target += forward * moveDist;
        if (m_keyS) m_target -= forward * moveDist;
        if (m_keyD) m_target += right   * moveDist;
        if (m_keyA) m_target -= right   * moveDist;

        const float rotSpeed = 1.5f;
        if (m_keyQ) m_yaw -= rotSpeed * dt;
        if (m_keyE) m_yaw += rotSpeed * dt;
    }
    else
    {
        // Free orbit: WASD = XZ pan, Q/E = up/down
        if (m_keyW) m_target += forward * moveDist;
        if (m_keyS) m_target -= forward * moveDist;
        if (m_keyD) m_target += right   * moveDist;
        if (m_keyA) m_target -= right   * moveDist;
        if (m_keyE) m_target.y += moveDist;
        if (m_keyQ) m_target.y -= moveDist;
    }

    UpdateMatrices();
}

void Map3D::UpdateEdgeScroll(int mouseX, int mouseY, float dt)
{
    if (!m_rtsMode) return;
    // Only scroll when the cursor is inside the window
    if (mouseX < 0 || mouseY < 0 ||
        mouseX >= m_screenWidth || mouseY >= m_screenHeight)
        return;

    D3DXVECTOR3 forward(-sinf(m_yaw), 0, -cosf(m_yaw));
    D3DXVECTOR3 right(-cosf(m_yaw), 0, sinf(m_yaw));

    float moveDist = m_moveSpeed * dt;
    float m = m_edgeScrollMargin;

    if (mouseX < m)                      m_target -= right   * moveDist;
    if (mouseX > m_screenWidth - m)      m_target += right   * moveDist;
    if (mouseY < m)                      m_target += forward * moveDist;
    if (mouseY > m_screenHeight - m)     m_target -= forward * moveDist;
}

void Map3D::SetFogUniforms(ID3DXEffect* effect) const
{
    if (!effect) return;
    D3DXVECTOR3 ep = m_eyePos;
    effect->SetValue("eyePos", &ep, sizeof(D3DXVECTOR3));
    effect->SetFloat("fogStart",  m_fogStart);
    effect->SetFloat("fogEnd",    m_fogEnd);
    effect->SetFloat("fogAmount", m_fogEnabled ? 1.0f : 0.0f);

    // Extract fog color RGB
    float r = ((m_fogColor >> 16) & 0xFF) / 255.0f;
    float g = ((m_fogColor >> 8)  & 0xFF) / 255.0f;
    float b = ( m_fogColor        & 0xFF) / 255.0f;
    D3DXVECTOR3 fc(r, g, b);
    effect->SetValue("fogColor", &fc, sizeof(D3DXVECTOR3));
}

void Map3D::Render()
{
    if (!m_pDev || !m_pVB || !m_pIB || !m_pTerrainEffect) return;

    // Clear background to fog color for horizon blend (always, even if fog off use gray)
    DWORD clearCol = m_fogEnabled ? m_fogColor : D3DCOLOR_ARGB(0, 30, 30, 30);
    m_pDev->Clear(0, nullptr, D3DCLEAR_TARGET, clearCol, 1.0f, 0);

    // Still set transforms for other renderers that read D3DTS_VIEW/D3DTS_PROJECTION
    // (UnitManager uses GetTransform to compute its own WVP)
    m_pDev->SetTransform(D3DTS_WORLD,      &m_matWorld);
    m_pDev->SetTransform(D3DTS_VIEW,       &m_matView);
    m_pDev->SetTransform(D3DTS_PROJECTION, &m_matProj);

    // Shader-based rendering (no fixed-function lighting/fog/texture stages)
    D3DXMATRIX wvp = m_matWorld * m_matView * m_matProj;
    m_pTerrainEffect->SetMatrix("matWVP",   &wvp);
    m_pTerrainEffect->SetMatrix("matWorld", &m_matWorld);
    SetFogUniforms(m_pTerrainEffect);
    m_pTerrainEffect->SetTexture("SourceTex", m_pCheckerTex);

    m_pTerrainEffect->SetTechnique(m_wireframe ? "Tech_Terrain_Wireframe" : "Tech_Terrain_Solid");

    m_pDev->SetStreamSource(0, m_pVB, 0, sizeof(MapVertex));
    m_pDev->SetIndices(m_pIB);
    m_pDev->SetFVF(MAP_FVF);

    UINT passes = 0;
    m_pTerrainEffect->Begin(&passes, 0);
    m_pTerrainEffect->BeginPass(0);
    m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
        m_vertexCount, 0, m_indexCount / 3);
    m_pTerrainEffect->EndPass();
    m_pTerrainEffect->End();
}

// --- Input handling ---

void Map3D::OnMouseDown(int button, int x, int y)
{
    if (button == 0) // left
    {
        m_dragging   = true;
        m_lastMouseX = x;
        m_lastMouseY = y;
    }
}

void Map3D::OnMouseUp(int button, int /*x*/, int /*y*/)
{
    if (button == 0)
        m_dragging = false;
}

void Map3D::OnMouseMove(int x, int y)
{
    if (m_rtsMode) return;  // no drag-rotate in RTS mode
    if (!m_dragging) return;

    int dx = x - m_lastMouseX;
    int dy = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;

    const float sensitivity = 0.005f;
    m_yaw   -= dx * sensitivity;
    m_pitch -= dy * sensitivity;
}

void Map3D::OnMouseWheel(int delta)
{
    // delta is typically +/- 120 per notch
    float step = m_distance * 0.1f;
    m_distance -= (delta / 120.0f) * step;
    if (m_distance < 1.0f)   m_distance = 1.0f;
    if (m_distance > 500.0f) m_distance = 500.0f;
}

void Map3D::OnKeyDown(int vkey)
{
    switch (vkey)
    {
    case 'W': m_keyW = true; break;
    case 'A': m_keyA = true; break;
    case 'S': m_keyS = true; break;
    case 'D': m_keyD = true; break;
    case 'Q': m_keyQ = true; break;
    case 'E': m_keyE = true; break;
    }
}

void Map3D::ScreenToRay(int sx, int sy, D3DXVECTOR3* origin, D3DXVECTOR3* dir) const
{
    D3DVIEWPORT9 vp;
    vp.X = 0; vp.Y = 0;
    vp.Width  = m_screenWidth;
    vp.Height = m_screenHeight;
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;

    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);

    D3DXVECTOR3 vNear((float)sx, (float)sy, 0.0f);
    D3DXVECTOR3 vFar ((float)sx, (float)sy, 1.0f);
    D3DXVECTOR3 wNear, wFar;
    D3DXVec3Unproject(&wNear, &vNear, &vp, &m_matProj, &m_matView, &identity);
    D3DXVec3Unproject(&wFar,  &vFar,  &vp, &m_matProj, &m_matView, &identity);

    *origin = wNear;
    D3DXVECTOR3 d = wFar - wNear;
    D3DXVec3Normalize(dir, &d);
}

bool Map3D::WorldToScreen(const D3DXVECTOR3& world, int* sx, int* sy) const
{
    D3DVIEWPORT9 vp;
    vp.X = 0; vp.Y = 0;
    vp.Width  = m_screenWidth;
    vp.Height = m_screenHeight;
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;

    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);

    D3DXVECTOR3 screen;
    D3DXVec3Project(&screen, &world, &vp, &m_matProj, &m_matView, &identity);

    // z outside [0,1] means behind / beyond frustum
    if (screen.z < 0 || screen.z > 1) return false;
    *sx = (int)screen.x;
    *sy = (int)screen.y;
    return true;
}

bool Map3D::ScreenToGround(int sx, int sy, D3DXVECTOR3* outHit) const
{
    D3DXVECTOR3 origin, dir;
    ScreenToRay(sx, sy, &origin, &dir);

    if (fabsf(dir.y) < 1e-6f) return false;  // parallel to ground
    float t = -origin.y / dir.y;
    if (t < 0) return false;                 // hit is behind camera

    *outHit = origin + dir * t;
    return true;
}

void Map3D::OnKeyUp(int vkey)
{
    switch (vkey)
    {
    case 'W': m_keyW = false; break;
    case 'A': m_keyA = false; break;
    case 'S': m_keyS = false; break;
    case 'D': m_keyD = false; break;
    case 'Q': m_keyQ = false; break;
    case 'E': m_keyE = false; break;
    }
}

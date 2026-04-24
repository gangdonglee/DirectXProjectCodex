#include "Map3D.h"
#include <d3dcompiler.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

struct TerrainCB
{
    D3DXMATRIX matWVP;
    D3DXMATRIX matWorld;
    D3DXVECTOR4 eyePos;
    D3DXVECTOR4 fogColor;
    D3DXVECTOR4 fogParams; // start, end, amount, unused
};

template <class T>
static void ReleaseCOM(T*& p)
{
    if (p) { p->Release(); p = nullptr; }
}

static std::wstring ToWidePath(const char* path)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wide[0], count);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
}

static bool CompileShaderFile(const char* path, const char* entry, const char* target, ID3DBlob** blob)
{
    ID3DBlob* errors = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    std::string resolved = path;
    std::wstring wide = ToWidePath(resolved.c_str());
    HRESULT hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, blob, &errors);
    if (FAILED(hr))
    {
        if (errors) { errors->Release(); errors = nullptr; }
        std::string name = path;
        size_t slash = name.find_last_of("\\/");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        resolved = "assets\\Shaders\\" + name;
        wide = ToWidePath(resolved.c_str());
        hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target, flags, 0, blob, &errors);
    }
    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(nullptr, (const char*)errors->GetBufferPointer(), path, MB_OK);
            errors->Release();
        }
        return false;
    }
    if (errors) errors->Release();
    return true;
}

Map3D::Map3D()
    : m_pDev(nullptr), m_pCtx(nullptr), m_pVB(nullptr), m_pIB(nullptr),
      m_pCheckerTex(nullptr), m_pCheckerSRV(nullptr), m_pVS(nullptr), m_pPS(nullptr),
      m_pLayout(nullptr), m_pCB(nullptr), m_pSampler(nullptr), m_pSolidRS(nullptr), m_pWireRS(nullptr),
      m_gridSize(0), m_tileSize(0), m_vertexCount(0), m_indexCount(0),
      m_screenWidth(0), m_screenHeight(0),
      m_target(0, 0, 0), m_distance(50.0f),
      m_yaw(0.7f), m_pitch(0.6f),
      m_fov(D3DX_PI / 4.0f), m_nearZ(0.1f), m_farZ(500.0f),
      m_dragging(false), m_lastMouseX(0), m_lastMouseY(0),
      m_keyW(false), m_keyA(false), m_keyS(false), m_keyD(false),
      m_keyQ(false), m_keyE(false), m_moveSpeed(20.0f),
      m_rtsMode(true), m_rtsPitch(D3DX_PI * 0.35f),
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

bool Map3D::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width, int height, int gridSize, float tileSize,
                 const char* terrainShaderPath)
{
    m_pDev = dev;
    m_pCtx = ctx;
    m_screenWidth = width;
    m_screenHeight = height;
    m_gridSize = gridSize;
    m_tileSize = tileSize;
    m_target = D3DXVECTOR3(0, 0, 0);
    m_distance = gridSize * tileSize * 1.2f;

    if (!CreateGridMesh()) return false;
    if (!CreateCheckerTexture()) return false;

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    if (!CompileShaderFile(terrainShaderPath, "VS_Terrain", "vs_4_0", &vsBlob)) return false;
    if (!CompileShaderFile(terrainShaderPath, "PS_Terrain", "ps_4_0", &psBlob)) { vsBlob->Release(); return false; }

    HRESULT hr = m_pDev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pVS);
    if (FAILED(hr)) return false;
    hr = m_pDev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pPS);
    if (FAILED(hr)) return false;

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = m_pDev->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pLayout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(TerrainCB);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDev->CreateBuffer(&cbd, nullptr, &m_pCB))) return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_pDev->CreateSamplerState(&sd, &m_pSampler))) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    if (FAILED(m_pDev->CreateRasterizerState(&rd, &m_pSolidRS))) return false;
    rd.FillMode = D3D11_FILL_WIREFRAME;
    if (FAILED(m_pDev->CreateRasterizerState(&rd, &m_pWireRS))) return false;

    UpdateMatrices();
    return true;
}

void Map3D::Shutdown()
{
    ReleaseCOM(m_pWireRS);
    ReleaseCOM(m_pSolidRS);
    ReleaseCOM(m_pSampler);
    ReleaseCOM(m_pCB);
    ReleaseCOM(m_pLayout);
    ReleaseCOM(m_pPS);
    ReleaseCOM(m_pVS);
    ReleaseCOM(m_pCheckerSRV);
    ReleaseCOM(m_pCheckerTex);
    ReleaseCOM(m_pIB);
    ReleaseCOM(m_pVB);
}

bool Map3D::CreateGridMesh()
{
    int verticesPerSide = m_gridSize + 1;
    m_vertexCount = verticesPerSide * verticesPerSide;
    m_indexCount = m_gridSize * m_gridSize * 6;
    float half = (m_gridSize * m_tileSize) * 0.5f;

    std::vector<MapVertex> verts(m_vertexCount);
    for (int z = 0; z < verticesPerSide; z++)
    {
        for (int x = 0; x < verticesPerSide; x++)
        {
            MapVertex& v = verts[z * verticesPerSide + x];
            v.x = x * m_tileSize - half;
            v.y = 0.0f;
            v.z = z * m_tileSize - half;
            v.nx = 0; v.ny = 1; v.nz = 0;
            v.u = (float)x;
            v.v = (float)z;
        }
    }

    std::vector<unsigned int> indices(m_indexCount);
    int idx = 0;
    for (int z = 0; z < m_gridSize; z++)
    {
        for (int x = 0; x < m_gridSize; x++)
        {
            int topLeft = z * verticesPerSide + x;
            int topRight = z * verticesPerSide + (x + 1);
            int bottomLeft = (z + 1) * verticesPerSide + x;
            int bottomRight = (z + 1) * verticesPerSide + (x + 1);
            indices[idx++] = topLeft; indices[idx++] = bottomLeft; indices[idx++] = topRight;
            indices[idx++] = topRight; indices[idx++] = bottomLeft; indices[idx++] = bottomRight;
        }
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(verts.size() * sizeof(MapVertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init = { verts.data(), 0, 0 };
    if (FAILED(m_pDev->CreateBuffer(&bd, &init, &m_pVB))) return false;

    bd.ByteWidth = (UINT)(indices.size() * sizeof(unsigned int));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    init.pSysMem = indices.data();
    return SUCCEEDED(m_pDev->CreateBuffer(&bd, &init, &m_pIB));
}

bool Map3D::CreateCheckerTexture()
{
    const int texSize = 64;
    std::vector<unsigned int> pixels(texSize * texSize);
    for (int y = 0; y < texSize; y++)
    {
        for (int x = 0; x < texSize; x++)
        {
            bool border = (x < 2) || (x >= texSize - 2) || (y < 2) || (y >= texSize - 2);
            pixels[y * texSize + x] = border ? D3DCOLOR_ARGB(255, 50, 50, 50) : D3DCOLOR_ARGB(255, 180, 200, 220);
        }
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = texSize;
    td.Height = texSize;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = { pixels.data(), texSize * sizeof(unsigned int), 0 };
    if (FAILED(m_pDev->CreateTexture2D(&td, &init, &m_pCheckerTex))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = td.Format;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    return SUCCEEDED(m_pDev->CreateShaderResourceView(m_pCheckerTex, &svd, &m_pCheckerSRV));
}

void Map3D::UpdateMatrices()
{
    const float maxPitch = D3DX_PI * 0.49f;
    if (m_pitch > maxPitch) m_pitch = maxPitch;
    if (m_pitch < -maxPitch) m_pitch = -maxPitch;

    float cosP = cosf(m_pitch);
    D3DXVECTOR3 offset(
        m_distance * cosP * sinf(m_yaw),
        m_distance * sinf(m_pitch),
        m_distance * cosP * cosf(m_yaw));
    D3DXVECTOR3 eye = m_target + offset;
    m_eyePos = eye;
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtLH(&m_matView, &eye, &m_target, &up);
    D3DXMatrixPerspectiveFovLH(&m_matProj, m_fov, (float)m_screenWidth / (float)m_screenHeight, m_nearZ, m_farZ);
    D3DXMatrixIdentity(&m_matWorld);
}

void Map3D::Update(float dt)
{
    float moveDist = m_moveSpeed * dt;
    D3DXVECTOR3 forward(-sinf(m_yaw), 0, -cosf(m_yaw));
    D3DXVECTOR3 right(-cosf(m_yaw), 0, sinf(m_yaw));

    if (m_rtsMode)
    {
        m_pitch = m_rtsPitch;
        if (m_keyW) m_target += forward * moveDist;
        if (m_keyS) m_target -= forward * moveDist;
        if (m_keyD) m_target += right * moveDist;
        if (m_keyA) m_target -= right * moveDist;
        const float rotSpeed = 1.5f;
        if (m_keyQ) m_yaw -= rotSpeed * dt;
        if (m_keyE) m_yaw += rotSpeed * dt;
    }
    else
    {
        if (m_keyW) m_target += forward * moveDist;
        if (m_keyS) m_target -= forward * moveDist;
        if (m_keyD) m_target += right * moveDist;
        if (m_keyA) m_target -= right * moveDist;
        if (m_keyE) m_target.y += moveDist;
        if (m_keyQ) m_target.y -= moveDist;
    }
    UpdateMatrices();
}

void Map3D::UpdateEdgeScroll(int mouseX, int mouseY, float dt)
{
    if (!m_rtsMode) return;
    if (mouseX < 0 || mouseY < 0 || mouseX >= m_screenWidth || mouseY >= m_screenHeight) return;
    D3DXVECTOR3 forward(-sinf(m_yaw), 0, -cosf(m_yaw));
    D3DXVECTOR3 right(-cosf(m_yaw), 0, sinf(m_yaw));
    float moveDist = m_moveSpeed * dt;
    float m = m_edgeScrollMargin;
    if (mouseX < m) m_target -= right * moveDist;
    if (mouseX > m_screenWidth - m) m_target += right * moveDist;
    if (mouseY < m) m_target += forward * moveDist;
    if (mouseY > m_screenHeight - m) m_target -= forward * moveDist;
}

void Map3D::Render()
{
    if (!m_pCtx || !m_pVB || !m_pIB || !m_pVS || !m_pPS) return;

    TerrainCB cb = {};
    cb.matWorld = m_matWorld;
    cb.matWVP = m_matWorld * m_matView * m_matProj;
    cb.eyePos = D3DXVECTOR4(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
    cb.fogColor = D3DXVECTOR4(((m_fogColor >> 16) & 0xff) / 255.0f,
                              ((m_fogColor >> 8) & 0xff) / 255.0f,
                              (m_fogColor & 0xff) / 255.0f, 1.0f);
    cb.fogParams = D3DXVECTOR4(m_fogStart, m_fogEnd, m_fogEnabled ? 1.0f : 0.0f, 0.0f);
    m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);

    UINT stride = sizeof(MapVertex);
    UINT offset = 0;
    m_pCtx->IASetInputLayout(m_pLayout);
    m_pCtx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
    m_pCtx->IASetIndexBuffer(m_pIB, DXGI_FORMAT_R32_UINT, 0);
    m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCtx->VSSetShader(m_pVS, nullptr, 0);
    m_pCtx->VSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetShader(m_pPS, nullptr, 0);
    m_pCtx->PSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetShaderResources(0, 1, &m_pCheckerSRV);
    m_pCtx->PSSetSamplers(0, 1, &m_pSampler);
    m_pCtx->RSSetState(m_wireframe ? m_pWireRS : m_pSolidRS);
    m_pCtx->DrawIndexed((UINT)m_indexCount, 0, 0);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pCtx->PSSetShaderResources(0, 1, &nullSRV);
}

void Map3D::OnMouseDown(int button, int x, int y)
{
    if (button == 0) { m_dragging = true; m_lastMouseX = x; m_lastMouseY = y; }
}

void Map3D::OnMouseUp(int button, int, int)
{
    if (button == 0) m_dragging = false;
}

void Map3D::OnMouseMove(int x, int y)
{
    if (m_rtsMode || !m_dragging) return;
    int dx = x - m_lastMouseX;
    int dy = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;
    const float sensitivity = 0.005f;
    m_yaw -= dx * sensitivity;
    m_pitch -= dy * sensitivity;
}

void Map3D::OnMouseWheel(int delta)
{
    float step = m_distance * 0.1f;
    m_distance -= (delta / 120.0f) * step;
    if (m_distance < 1.0f) m_distance = 1.0f;
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

void Map3D::ScreenToRay(int sx, int sy, D3DXVECTOR3* origin, D3DXVECTOR3* dir) const
{
    D3DXVECTOR3 nearPt((float)sx, (float)sy, 0.0f);
    D3DXVECTOR3 farPt((float)sx, (float)sy, 1.0f);
    D3DXVECTOR3 wNear, wFar;
    UnprojectPoint(nearPt, m_matView, m_matProj, m_screenWidth, m_screenHeight, &wNear);
    UnprojectPoint(farPt, m_matView, m_matProj, m_screenWidth, m_screenHeight, &wFar);
    *origin = wNear;
    D3DXVECTOR3 d = wFar - wNear;
    D3DXVec3Normalize(dir, &d);
}

bool Map3D::WorldToScreen(const D3DXVECTOR3& world, int* sx, int* sy) const
{
    D3DXVECTOR3 screen;
    if (!ProjectPoint(world, m_matView, m_matProj, m_screenWidth, m_screenHeight, &screen)) return false;
    *sx = (int)screen.x;
    *sy = (int)screen.y;
    return true;
}

bool Map3D::ScreenToGround(int sx, int sy, D3DXVECTOR3* outHit) const
{
    D3DXVECTOR3 origin, dir;
    ScreenToRay(sx, sy, &origin, &dir);
    if (fabsf(dir.y) < 1e-6f) return false;
    float t = -origin.y / dir.y;
    if (t < 0) return false;
    *outHit = origin + dir * t;
    return true;
}

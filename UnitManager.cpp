#include "UnitManager.h"
#include "Map3D.h"
#include <d3dcompiler.h>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

struct UnitCB
{
    D3DXMATRIX matWVP;
    D3DXMATRIX matWorld;
    D3DXVECTOR4 color;
    D3DXVECTOR4 data; // time, progress/hitFlash, mode, unused
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
    std::wstring wide = ToWidePath(path);
    HRESULT hr = D3DCompileFromFile(wide.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, blob, &errors);
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

static D3DXVECTOR4 ColorFromDWORD(DWORD c)
{
    return D3DXVECTOR4(((c >> 16) & 0xff) / 255.0f,
                       ((c >> 8) & 0xff) / 255.0f,
                       (c & 0xff) / 255.0f,
                       ((c >> 24) & 0xff) / 255.0f);
}

UnitManager::UnitManager()
    : m_pDev(nullptr), m_pCtx(nullptr), m_pVB(nullptr), m_pIB(nullptr), m_pMarkerVB(nullptr),
      m_pUnitVS(nullptr), m_pUnitPS(nullptr), m_pMarkerVS(nullptr), m_pMarkerPS(nullptr), m_pSelectionPS(nullptr),
      m_pUnitLayout(nullptr), m_pMarkerLayout(nullptr), m_pCB(nullptr), m_pSolidRS(nullptr),
      m_pNoCullRS(nullptr), m_pAlphaBlend(nullptr), m_pDepthOn(nullptr), m_time(0.0f),
      m_vertexCount(0), m_indexCount(0), m_moveSpeed(8.0f)
{
}

UnitManager::~UnitManager()
{
    Shutdown();
}

bool UnitManager::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                       const char* unitShaderPath,
                       const char* markerShaderPath,
                       const char* selectionShaderPath)
{
    m_pDev = dev;
    m_pCtx = ctx;
    if (!CreateCubeMesh()) return false;
    if (!CreateMarkerMesh()) return false;

    ID3DBlob* unitVS = nullptr;
    ID3DBlob* unitPS = nullptr;
    ID3DBlob* markerVS = nullptr;
    ID3DBlob* markerPS = nullptr;
    ID3DBlob* selectionPS = nullptr;
    if (!CompileShaderFile(unitShaderPath, "VS_Unit", "vs_4_0", &unitVS)) return false;
    if (!CompileShaderFile(unitShaderPath, "PS_Unit", "ps_4_0", &unitPS)) return false;
    if (!CompileShaderFile(markerShaderPath, "VS_Marker", "vs_4_0", &markerVS)) return false;
    if (!CompileShaderFile(markerShaderPath, "PS_Marker", "ps_4_0", &markerPS)) return false;
    if (!CompileShaderFile(selectionShaderPath, "PS_Selection", "ps_4_0", &selectionPS)) return false;

    if (FAILED(m_pDev->CreateVertexShader(unitVS->GetBufferPointer(), unitVS->GetBufferSize(), nullptr, &m_pUnitVS))) return false;
    if (FAILED(m_pDev->CreatePixelShader(unitPS->GetBufferPointer(), unitPS->GetBufferSize(), nullptr, &m_pUnitPS))) return false;
    if (FAILED(m_pDev->CreateVertexShader(markerVS->GetBufferPointer(), markerVS->GetBufferSize(), nullptr, &m_pMarkerVS))) return false;
    if (FAILED(m_pDev->CreatePixelShader(markerPS->GetBufferPointer(), markerPS->GetBufferSize(), nullptr, &m_pMarkerPS))) return false;
    if (FAILED(m_pDev->CreatePixelShader(selectionPS->GetBufferPointer(), selectionPS->GetBufferSize(), nullptr, &m_pSelectionPS))) return false;

    D3D11_INPUT_ELEMENT_DESC unitLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(m_pDev->CreateInputLayout(unitLayout, 2, unitVS->GetBufferPointer(), unitVS->GetBufferSize(), &m_pUnitLayout))) return false;

    D3D11_INPUT_ELEMENT_DESC markerLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(m_pDev->CreateInputLayout(markerLayout, 2, markerVS->GetBufferPointer(), markerVS->GetBufferSize(), &m_pMarkerLayout))) return false;

    unitVS->Release(); unitPS->Release(); markerVS->Release(); markerPS->Release(); selectionPS->Release();

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(UnitCB);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDev->CreateBuffer(&cbd, nullptr, &m_pCB))) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    if (FAILED(m_pDev->CreateRasterizerState(&rd, &m_pSolidRS))) return false;

    rd.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_pDev->CreateRasterizerState(&rd, &m_pNoCullRS))) return false;

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_pDev->CreateBlendState(&bd, &m_pAlphaBlend))) return false;

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(m_pDev->CreateDepthStencilState(&dd, &m_pDepthOn))) return false;

    return true;
}

void UnitManager::Shutdown()
{
    ReleaseCOM(m_pDepthOn);
    ReleaseCOM(m_pAlphaBlend);
    ReleaseCOM(m_pNoCullRS);
    ReleaseCOM(m_pSolidRS);
    ReleaseCOM(m_pCB);
    ReleaseCOM(m_pMarkerLayout);
    ReleaseCOM(m_pUnitLayout);
    ReleaseCOM(m_pMarkerPS);
    ReleaseCOM(m_pSelectionPS);
    ReleaseCOM(m_pMarkerVS);
    ReleaseCOM(m_pUnitPS);
    ReleaseCOM(m_pUnitVS);
    ReleaseCOM(m_pMarkerVB);
    ReleaseCOM(m_pIB);
    ReleaseCOM(m_pVB);
    m_units.clear();
    m_markers.clear();
}

bool UnitManager::CreateCubeMesh()
{
    const float s = 0.5f;
    DWORD col = 0xffffffff;
    CubeVertex verts[24] =
    {
        { +s, 0.0f, -s,  1, 0, 0, col }, { +s, 1.0f, -s,  1, 0, 0, col }, { +s, 1.0f, +s,  1, 0, 0, col }, { +s, 0.0f, +s,  1, 0, 0, col },
        { -s, 0.0f, +s, -1, 0, 0, col }, { -s, 1.0f, +s, -1, 0, 0, col }, { -s, 1.0f, -s, -1, 0, 0, col }, { -s, 0.0f, -s, -1, 0, 0, col },
        { -s, 1.0f, -s,  0, 1, 0, col }, { -s, 1.0f, +s,  0, 1, 0, col }, { +s, 1.0f, +s,  0, 1, 0, col }, { +s, 1.0f, -s,  0, 1, 0, col },
        { -s, 0.0f, +s,  0,-1, 0, col }, { -s, 0.0f, -s,  0,-1, 0, col }, { +s, 0.0f, -s,  0,-1, 0, col }, { +s, 0.0f, +s,  0,-1, 0, col },
        { +s, 0.0f, +s,  0, 0, 1, col }, { +s, 1.0f, +s,  0, 0, 1, col }, { -s, 1.0f, +s,  0, 0, 1, col }, { -s, 0.0f, +s,  0, 0, 1, col },
        { -s, 0.0f, -s,  0, 0,-1, col }, { -s, 1.0f, -s,  0, 0,-1, col }, { +s, 1.0f, -s,  0, 0,-1, col }, { +s, 0.0f, -s,  0, 0,-1, col },
    };

    unsigned int indices[36];
    for (int f = 0; f < 6; f++)
    {
        int base = f * 4;
        int o = f * 6;
        indices[o + 0] = base + 0; indices[o + 1] = base + 1; indices[o + 2] = base + 2;
        indices[o + 3] = base + 0; indices[o + 4] = base + 2; indices[o + 5] = base + 3;
    }
    m_vertexCount = 24;
    m_indexCount = 36;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(verts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init = { verts, 0, 0 };
    if (FAILED(m_pDev->CreateBuffer(&bd, &init, &m_pVB))) return false;

    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    init.pSysMem = indices;
    return SUCCEEDED(m_pDev->CreateBuffer(&bd, &init, &m_pIB));
}

bool UnitManager::CreateMarkerMesh()
{
    const float s = 1.0f;
    MarkerVertex verts[4] =
    {
        { -s, 0.05f, -s, 0, 0 },
        { +s, 0.05f, -s, 1, 0 },
        { -s, 0.05f, +s, 0, 1 },
        { +s, 0.05f, +s, 1, 1 },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(verts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA init = { verts, 0, 0 };
    return SUCCEEDED(m_pDev->CreateBuffer(&bd, &init, &m_pMarkerVB));
}

void UnitManager::AddMarker(const D3DXVECTOR3& pos, float life)
{
    Marker m;
    m.pos = pos;
    m.life = life;
    m.maxLife = life;
    m_markers.push_back(m);
}

Unit& UnitManager::Add(const D3DXVECTOR3& pos)
{
    Unit u;
    u.position = pos;
    u.targetPos = pos;
    m_units.push_back(u);
    return m_units.back();
}

void UnitManager::Clear()
{
    m_units.clear();
}

void UnitManager::Update(float dt)
{
    m_time += dt;
    for (auto it = m_markers.begin(); it != m_markers.end(); )
    {
        it->life -= dt;
        if (it->life <= 0) it = m_markers.erase(it);
        else ++it;
    }

    for (size_t i = 0; i < m_units.size(); i++)
    {
        Unit& u = m_units[i];
        if (!u.alive) continue;
        if (u.highlightTimer > 0) u.highlightTimer -= dt;
        if (u.hitFlashTimer > 0) u.hitFlashTimer -= dt;

        bool attacking = false;
        if (u.attackTarget >= 0 && u.attackTarget < (int)m_units.size())
        {
            Unit& t = m_units[u.attackTarget];
            if (!t.alive) u.attackTarget = -1;
            else
            {
                D3DXVECTOR3 toT = t.position - u.position;
                toT.y = 0;
                float dist = D3DXVec3Length(&toT);

                int attackerCount = 0;
                int slotIndex = 0;
                for (size_t k = 0; k < m_units.size(); k++)
                {
                    const Unit& other = m_units[k];
                    if (!other.alive || other.attackTarget != u.attackTarget) continue;
                    if (k < i) slotIndex++;
                    attackerCount++;
                }

                float ringRadius = t.collisionRadius + u.collisionRadius + 0.65f;
                if (attackerCount > 1)
                {
                    float desiredArcSpacing = u.collisionRadius * 2.0f + 0.45f;
                    float countRadius = (attackerCount * desiredArcSpacing) / (2.0f * D3DX_PI);
                    if (ringRadius < countRadius) ringRadius = countRadius;
                }
                float maxAttackStandRadius = u.attackRange * 0.85f;
                if (ringRadius > maxAttackStandRadius) ringRadius = maxAttackStandRadius;

                float angle = (attackerCount > 0)
                    ? (2.0f * D3DX_PI * (float)slotIndex / (float)attackerCount)
                    : 0.0f;
                D3DXVECTOR3 attackSlot(
                    t.position.x + cosf(angle) * ringRadius,
                    t.position.y,
                    t.position.z + sinf(angle) * ringRadius);

                D3DXVECTOR3 toSlot = attackSlot - u.position;
                toSlot.y = 0;
                float slotDist = D3DXVec3Length(&toSlot);
                const float slotTolerance = 0.25f;

                if (dist > u.attackRange || slotDist > slotTolerance)
                {
                    u.targetPos = attackSlot;
                }
                else
                {
                    u.targetPos = u.position;
                    attacking = true;
                    if (dist > 0.01f)
                    {
                        D3DXVECTOR3 dir = toT / dist;
                        u.yaw = atan2f(dir.x, dir.z);
                    }
                    if (t.highlightTimer < 1.0f) t.highlightTimer = 1.0f;
                    u.attackTimer -= dt;
                    if (u.attackTimer <= 0)
                    {
                        t.hp -= u.attackDamage;
                        t.hitFlashTimer = 0.22f;
                        u.attackTimer = u.attackInterval;
                        if (t.hp <= 0)
                        {
                            t.hp = 0;
                            t.alive = false;
                            u.attackTarget = -1;
                        }
                    }
                }
            }
        }

        if (!attacking)
        {
            D3DXVECTOR3 diff = u.targetPos - u.position;
            diff.y = 0;
            float d = D3DXVec3Length(&diff);
            float arriveRadius = u.collisionRadius * 0.25f;
            if (arriveRadius < 0.08f) arriveRadius = 0.08f;
            if (d > arriveRadius)
            {
                u.moving = true;
                D3DXVECTOR3 dir = diff / d;
                u.yaw = atan2f(dir.x, dir.z);
                float step = m_moveSpeed * dt;
                float move = d - arriveRadius;
                if (step > move) step = move;
                u.position += dir * step;
            }
            else u.moving = false;
        }
    }

    ApplySeparation(dt);
}

void UnitManager::ApplySeparation(float dt)
{
    const float stiffness = 6.0f;
    float factor = stiffness * dt;
    if (factor > 1.0f) factor = 1.0f;

    for (size_t i = 0; i < m_units.size(); i++)
    {
        Unit& a = m_units[i];
        if (!a.alive) continue;

        for (size_t j = i + 1; j < m_units.size(); j++)
        {
            Unit& b = m_units[j];
            if (!b.alive) continue;

            D3DXVECTOR3 delta = b.position - a.position;
            delta.y = 0.0f;
            float minDist = a.collisionRadius + b.collisionRadius;
            float distSq = delta.x * delta.x + delta.z * delta.z;
            if (distSq >= minDist * minDist) continue;

            float dist = sqrtf(distSq);
            D3DXVECTOR3 dir;
            if (dist > 0.0001f)
            {
                dir = delta / dist;
            }
            else
            {
                float angle = (float)((i * 37 + j * 17) % 360) * (D3DX_PI / 180.0f);
                dir = D3DXVECTOR3(cosf(angle), 0.0f, sinf(angle));
                dist = 0.0f;
            }

            float push = (minDist - dist) * 0.5f * factor;
            D3DXVECTOR3 offset = dir * push;
            a.position -= offset;
            b.position += offset;
        }
    }
}

int UnitManager::PickUnit(const D3DXVECTOR3& rayOrigin, const D3DXVECTOR3& rayDir) const
{
    int bestIdx = -1;
    float bestT = FLT_MAX;
    for (size_t i = 0; i < m_units.size(); i++)
    {
        const Unit& u = m_units[i];
        if (!u.alive) continue;
        D3DXVECTOR3 mn(u.position.x - 0.5f, u.position.y, u.position.z - 0.5f);
        D3DXVECTOR3 mx(u.position.x + 0.5f, u.position.y + 1.0f, u.position.z + 0.5f);
        float tmin = -FLT_MAX, tmax = FLT_MAX;
        for (int a = 0; a < 3; a++)
        {
            float ro = (&rayOrigin.x)[a];
            float rd = (&rayDir.x)[a];
            float lo = (&mn.x)[a];
            float hi = (&mx.x)[a];
            if (fabsf(rd) < 1e-6f)
            {
                if (ro < lo || ro > hi) { tmin = FLT_MAX; break; }
            }
            else
            {
                float t1 = (lo - ro) / rd;
                float t2 = (hi - ro) / rd;
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) { tmin = FLT_MAX; break; }
            }
        }
        if (tmin != FLT_MAX && tmin >= 0 && tmin < bestT)
        {
            bestT = tmin;
            bestIdx = (int)i;
        }
    }
    return bestIdx;
}

void UnitManager::ClearSelection()
{
    for (auto& u : m_units) u.selected = false;
}

void UnitManager::SetSelected(int index, bool sel)
{
    if (index >= 0 && index < (int)m_units.size()) m_units[index].selected = sel;
}

int UnitManager::SelectedCount() const
{
    int c = 0;
    for (const auto& u : m_units) if (u.selected) c++;
    return c;
}

void UnitManager::MoveSelectedTo(const D3DXVECTOR3& pos)
{
    int selCount = SelectedCount();
    if (selCount == 0) return;

    const float spacing = 1.15f;
    int columns = (int)ceilf(sqrtf((float)selCount));
    int rows = (selCount + columns - 1) / columns;

    int idx = 0;
    for (auto& u : m_units)
    {
        if (!u.selected || !u.alive) continue;

        int col = idx % columns;
        int row = idx / columns;
        float x = ((float)col - (float)(columns - 1) * 0.5f) * spacing;
        float z = ((float)row - (float)(rows - 1) * 0.5f) * spacing;

        u.targetPos = D3DXVECTOR3(pos.x + x, pos.y, pos.z + z);
        u.attackTarget = -1;
        idx++;
    }
}

void UnitManager::AttackCommand(int targetIdx)
{
    if (targetIdx < 0 || targetIdx >= (int)m_units.size()) return;
    if (!m_units[targetIdx].alive) return;
    m_units[targetIdx].highlightTimer = 2.0f;
    for (auto& u : m_units)
    {
        if (!u.selected || !u.alive) continue;
        u.attackTarget = targetIdx;
    }
}

void UnitManager::Render(const Map3D& cam)
{
    if (!m_pCtx || !m_pVB || !m_pIB || m_units.empty()) return;

    D3DXMATRIX viewProj = cam.GetViewMatrix() * cam.GetProjMatrix();
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pCtx->OMSetDepthStencilState(m_pDepthOn, 0);
    m_pCtx->RSSetState(m_pSolidRS);
    m_pCtx->IASetInputLayout(m_pUnitLayout);
    m_pCtx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
    m_pCtx->IASetIndexBuffer(m_pIB, DXGI_FORMAT_R32_UINT, 0);
    m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCtx->VSSetShader(m_pUnitVS, nullptr, 0);
    m_pCtx->PSSetShader(m_pUnitPS, nullptr, 0);
    m_pCtx->VSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->PSSetConstantBuffers(0, 1, &m_pCB);

    for (const auto& u : m_units)
    {
        if (!u.alive) continue;
        D3DXMATRIX rot, trans, world;
        D3DXMatrixRotationY(&rot, u.yaw);
        D3DXMatrixTranslation(&trans, u.position.x, u.position.y, u.position.z);
        world = rot * trans;

        DWORD tint;
        if (u.selected) tint = D3DCOLOR_ARGB(255, 255, 230, 80);
        else if (u.team == 0) tint = D3DCOLOR_ARGB(255, 200, 80, 80);
        else tint = D3DCOLOR_ARGB(255, 80, 120, 240);
        D3DXVECTOR4 color = ColorFromDWORD(tint);
        UnitCB cb = {};
        cb.matWorld = world;
        cb.matWVP = world * viewProj;
        cb.color = color;
        cb.data = D3DXVECTOR4(m_time, u.hitFlashTimer > 0 ? (u.hitFlashTimer / 0.22f) : 0.0f, 0, 0);
        m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);
        m_pCtx->DrawIndexed((UINT)m_indexCount, 0, 0);
    }

    stride = sizeof(MarkerVertex);
    m_pCtx->IASetInputLayout(m_pMarkerLayout);
    m_pCtx->IASetVertexBuffers(0, 1, &m_pMarkerVB, &stride, &offset);
    m_pCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pCtx->VSSetShader(m_pMarkerVS, nullptr, 0);
    m_pCtx->PSSetConstantBuffers(0, 1, &m_pCB);
    m_pCtx->RSSetState(m_pNoCullRS);
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pCtx->OMSetBlendState(m_pAlphaBlend, blendFactor, 0xffffffff);

    for (const auto& u : m_units)
    {
        if (!u.alive) continue;
        bool wantGreen = u.selected;
        bool wantRed = u.highlightTimer > 0;
        if (!wantGreen && !wantRed) continue;

        D3DXMATRIX s, trans, world;
        D3DXMatrixScaling(&s, 0.8f, 1.0f, 0.8f);
        D3DXMatrixTranslation(&trans, u.position.x, u.position.y, u.position.z);
        world = s * trans;

        UnitCB cb = {};
        cb.matWorld = world;
        cb.matWVP = world * viewProj;
        cb.data = D3DXVECTOR4(m_time, 1, 0, 0);
        m_pCtx->PSSetShader(m_pSelectionPS, nullptr, 0);
        if (wantGreen)
        {
            cb.color = D3DXVECTOR4(0.25f, 1.0f, 0.3f, 0.9f);
            m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);
            m_pCtx->Draw(4, 0);
        }
        if (wantRed)
        {
            cb.color = D3DXVECTOR4(1.0f, 0.25f, 0.25f, 0.9f);
            m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);
            m_pCtx->Draw(4, 0);
        }
    }

    for (const auto& mk : m_markers)
    {
        float t = mk.life / mk.maxLife;
        float sz = 1.0f + (1.0f - t) * 1.5f;
        D3DXMATRIX s, trans, world;
        D3DXMatrixScaling(&s, sz, 1.0f, sz);
        D3DXMatrixTranslation(&trans, mk.pos.x, mk.pos.y, mk.pos.z);
        world = s * trans;
        UnitCB cb = {};
        cb.matWorld = world;
        cb.matWVP = world * viewProj;
        cb.color = D3DXVECTOR4(1.0f, 0.2f, 0.2f, 1.0f);
        cb.data = D3DXVECTOR4(m_time, t, 1, 0);
        m_pCtx->UpdateSubresource(m_pCB, 0, nullptr, &cb, 0, 0);
        m_pCtx->PSSetShader(m_pMarkerPS, nullptr, 0);
        m_pCtx->Draw(4, 0);
    }

    m_pCtx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pCtx->RSSetState(m_pSolidRS);
}

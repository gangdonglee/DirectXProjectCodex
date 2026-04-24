#include "UnitManager.h"
#include "Map3D.h"
#include <d3dcompiler.h>
#include <cmath>
#include <cfloat>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

struct UnitCB
{
    D3DXMATRIX matWVP;
    D3DXMATRIX matWorld;
    D3DXVECTOR4 color;
    D3DXVECTOR4 data; // time, progress, mode, unused
};

static const char* kUnitShader = R"(
#pragma pack_matrix(row_major)
cbuffer UnitCB : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
    float4 color;
    float4 data;
};

struct VS_IN { float3 pos : POSITION; float3 normal : NORMAL; };
struct VS_OUT { float4 pos : SV_POSITION; float3 normal : TEXCOORD0; };

VS_OUT VSMain(VS_IN i)
{
    VS_OUT o;
    o.pos = mul(float4(i.pos, 1), matWVP);
    o.normal = normalize(mul(float4(i.normal, 0), matWorld).xyz);
    return o;
}

float4 PSMain(VS_OUT i) : SV_TARGET
{
    float3 lightDir = normalize(float3(-0.35, 0.85, -0.35));
    float ndl = saturate(dot(normalize(i.normal), lightDir));
    float3 lit = color.rgb * (0.35 + ndl * 0.75);
    return float4(lit, color.a);
}

struct MVS_IN { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct MVS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

MVS_OUT MarkerVS(MVS_IN i)
{
    MVS_OUT o;
    o.pos = mul(float4(i.pos, 1), matWVP);
    o.uv = i.uv;
    return o;
}

float4 RingPS(MVS_OUT i) : SV_TARGET
{
    float2 p = i.uv * 2.0 - 1.0;
    float d = length(p);
    if (data.z > 0.5)
    {
        float markerRing = smoothstep(0.95, 0.82, d) * smoothstep(0.48, 0.62, d);
        return float4(color.rgb, markerRing * saturate(data.y));
    }
    float pulse = 0.06 * sin(data.x * 8.0);
    float a = smoothstep(0.92 + pulse, 0.82 + pulse, d) * smoothstep(0.58, 0.70, d);
    return float4(color.rgb, a * color.a);
}
)";

template <class T>
static void ReleaseCOM(T*& p)
{
    if (p) { p->Release(); p = nullptr; }
}

static bool CompileShader(const char* entry, const char* target, ID3DBlob** blob)
{
    ID3DBlob* errors = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif
    HRESULT hr = D3DCompile(kUnitShader, strlen(kUnitShader), nullptr, nullptr, nullptr,
        entry, target, flags, 0, blob, &errors);
    if (FAILED(hr))
    {
        if (errors)
        {
            MessageBoxA(nullptr, (const char*)errors->GetBufferPointer(), "Unit Shader Error", MB_OK);
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
      m_pUnitVS(nullptr), m_pUnitPS(nullptr), m_pMarkerVS(nullptr), m_pMarkerPS(nullptr),
      m_pUnitLayout(nullptr), m_pMarkerLayout(nullptr), m_pCB(nullptr), m_pSolidRS(nullptr),
      m_pAlphaBlend(nullptr), m_pDepthOn(nullptr), m_time(0.0f),
      m_vertexCount(0), m_indexCount(0), m_moveSpeed(8.0f)
{
}

UnitManager::~UnitManager()
{
    Shutdown();
}

bool UnitManager::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                       const char*, const char*, const char*)
{
    m_pDev = dev;
    m_pCtx = ctx;
    if (!CreateCubeMesh()) return false;
    if (!CreateMarkerMesh()) return false;

    ID3DBlob* unitVS = nullptr;
    ID3DBlob* unitPS = nullptr;
    ID3DBlob* markerVS = nullptr;
    ID3DBlob* markerPS = nullptr;
    if (!CompileShader("VSMain", "vs_4_0", &unitVS)) return false;
    if (!CompileShader("PSMain", "ps_4_0", &unitPS)) return false;
    if (!CompileShader("MarkerVS", "vs_4_0", &markerVS)) return false;
    if (!CompileShader("RingPS", "ps_4_0", &markerPS)) return false;

    if (FAILED(m_pDev->CreateVertexShader(unitVS->GetBufferPointer(), unitVS->GetBufferSize(), nullptr, &m_pUnitVS))) return false;
    if (FAILED(m_pDev->CreatePixelShader(unitPS->GetBufferPointer(), unitPS->GetBufferSize(), nullptr, &m_pUnitPS))) return false;
    if (FAILED(m_pDev->CreateVertexShader(markerVS->GetBufferPointer(), markerVS->GetBufferSize(), nullptr, &m_pMarkerVS))) return false;
    if (FAILED(m_pDev->CreatePixelShader(markerPS->GetBufferPointer(), markerPS->GetBufferSize(), nullptr, &m_pMarkerPS))) return false;

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

    unitVS->Release(); unitPS->Release(); markerVS->Release(); markerPS->Release();

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
    ReleaseCOM(m_pSolidRS);
    ReleaseCOM(m_pCB);
    ReleaseCOM(m_pMarkerLayout);
    ReleaseCOM(m_pUnitLayout);
    ReleaseCOM(m_pMarkerPS);
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
                if (dist > u.attackRange) u.targetPos = t.position;
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
            if (d > 0.01f)
            {
                u.moving = true;
                D3DXVECTOR3 dir = diff / d;
                u.yaw = atan2f(dir.x, dir.z);
                float step = m_moveSpeed * dt;
                if (step >= d) u.position = u.targetPos;
                else u.position += dir * step;
            }
            else u.moving = false;
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
    int idx = 0;
    float spread = (selCount > 1) ? 0.8f : 0.0f;
    for (auto& u : m_units)
    {
        if (!u.selected || !u.alive) continue;
        float angle = (selCount > 1) ? (6.2831853f * idx / selCount) : 0;
        float off = spread * sqrtf((float)idx);
        u.targetPos = D3DXVECTOR3(pos.x + cosf(angle) * off, pos.y, pos.z + sinf(angle) * off);
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
        if (u.hitFlashTimer > 0)
        {
            float f = u.hitFlashTimer / 0.22f;
            if (f > 1) f = 1;
            color.x += (1.0f - color.x) * f;
            color.y += (1.0f - color.y) * f;
            color.z += (1.0f - color.z) * f;
        }

        UnitCB cb = {};
        cb.matWorld = world;
        cb.matWVP = world * viewProj;
        cb.color = color;
        cb.data = D3DXVECTOR4(m_time, 1, 0, 0);
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
        m_pCtx->PSSetShader(m_pMarkerPS, nullptr, 0);
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
        m_pCtx->Draw(4, 0);
    }

    m_pCtx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

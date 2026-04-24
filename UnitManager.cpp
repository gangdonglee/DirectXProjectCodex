#include "UnitManager.h"
#include "Map3D.h"
#include <cmath>
#include <cfloat>

#define UNIT_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE)

UnitManager::UnitManager()
    : m_pDev(nullptr), m_pVB(nullptr), m_pIB(nullptr),
      m_pMarkerVB(nullptr),
      m_pUnitEffect(nullptr), m_pMarkerEffect(nullptr),
      m_pSelectionEffect(nullptr), m_time(0.0f),
      m_vertexCount(0), m_indexCount(0), m_moveSpeed(8.0f)
{
}

UnitManager::~UnitManager()
{
    Shutdown();
}

bool UnitManager::Init(IDirect3DDevice9* dev,
                       const char* unitShaderPath,
                       const char* markerShaderPath,
                       const char* selectionShaderPath)
{
    m_pDev = dev;
    if (!CreateCubeMesh())   return false;
    if (!CreateMarkerMesh()) return false;

    ID3DXBuffer* err = nullptr;
    HRESULT hr = D3DXCreateEffectFromFileA(dev, unitShaderPath,
        nullptr, nullptr, 0, nullptr, &m_pUnitEffect, &err);
    if (FAILED(hr))
    {
        if (err)
        {
            MessageBoxA(nullptr, (char*)err->GetBufferPointer(), "Unit Shader Error", MB_OK);
            err->Release();
        }
        return false;
    }

    err = nullptr;
    hr = D3DXCreateEffectFromFileA(dev, markerShaderPath,
        nullptr, nullptr, 0, nullptr, &m_pMarkerEffect, &err);
    if (FAILED(hr))
    {
        if (err)
        {
            MessageBoxA(nullptr, (char*)err->GetBufferPointer(), "Marker Shader Error", MB_OK);
            err->Release();
        }
        return false;
    }

    err = nullptr;
    hr = D3DXCreateEffectFromFileA(dev, selectionShaderPath,
        nullptr, nullptr, 0, nullptr, &m_pSelectionEffect, &err);
    if (FAILED(hr))
    {
        if (err)
        {
            MessageBoxA(nullptr, (char*)err->GetBufferPointer(), "Selection Shader Error", MB_OK);
            err->Release();
        }
        return false;
    }
    return true;
}

void UnitManager::Shutdown()
{
    if (m_pSelectionEffect) { m_pSelectionEffect->Release(); m_pSelectionEffect = nullptr; }
    if (m_pMarkerEffect)    { m_pMarkerEffect->Release();    m_pMarkerEffect    = nullptr; }
    if (m_pMarkerVB)        { m_pMarkerVB->Release();        m_pMarkerVB        = nullptr; }
    if (m_pIB)              { m_pIB->Release();              m_pIB              = nullptr; }
    if (m_pVB)              { m_pVB->Release();              m_pVB              = nullptr; }
    m_units.clear();
    m_markers.clear();
}

bool UnitManager::CreateCubeMesh()
{
    // Unit cube centered at (0, 0.5, 0) so it stands on Y=0 plane
    const float s = 0.5f;
    DWORD col = 0xFFFFFFFF;

    CubeVertex verts[24] =
    {
        // +X face
        { +s, 0.0f, -s,  1, 0, 0, col }, { +s, 1.0f, -s,  1, 0, 0, col },
        { +s, 1.0f, +s,  1, 0, 0, col }, { +s, 0.0f, +s,  1, 0, 0, col },
        // -X face
        { -s, 0.0f, +s, -1, 0, 0, col }, { -s, 1.0f, +s, -1, 0, 0, col },
        { -s, 1.0f, -s, -1, 0, 0, col }, { -s, 0.0f, -s, -1, 0, 0, col },
        // +Y face (top)
        { -s, 1.0f, -s,  0, 1, 0, col }, { -s, 1.0f, +s,  0, 1, 0, col },
        { +s, 1.0f, +s,  0, 1, 0, col }, { +s, 1.0f, -s,  0, 1, 0, col },
        // -Y face (bottom)
        { -s, 0.0f, +s,  0,-1, 0, col }, { -s, 0.0f, -s,  0,-1, 0, col },
        { +s, 0.0f, -s,  0,-1, 0, col }, { +s, 0.0f, +s,  0,-1, 0, col },
        // +Z face
        { +s, 0.0f, +s,  0, 0, 1, col }, { +s, 1.0f, +s,  0, 0, 1, col },
        { -s, 1.0f, +s,  0, 0, 1, col }, { -s, 0.0f, +s,  0, 0, 1, col },
        // -Z face
        { -s, 0.0f, -s,  0, 0,-1, col }, { -s, 1.0f, -s,  0, 0,-1, col },
        { +s, 1.0f, -s,  0, 0,-1, col }, { +s, 0.0f, -s,  0, 0,-1, col },
    };

    DWORD indices[36];
    for (int f = 0; f < 6; f++)
    {
        int base = f * 4;
        int o = f * 6;
        indices[o + 0] = base + 0;
        indices[o + 1] = base + 1;
        indices[o + 2] = base + 2;
        indices[o + 3] = base + 0;
        indices[o + 4] = base + 2;
        indices[o + 5] = base + 3;
    }

    m_vertexCount = 24;
    m_indexCount  = 36;

    if (FAILED(m_pDev->CreateVertexBuffer(sizeof(verts), 0, UNIT_FVF,
        D3DPOOL_MANAGED, &m_pVB, nullptr)))
        return false;
    void* p;
    m_pVB->Lock(0, 0, &p, 0); memcpy(p, verts, sizeof(verts)); m_pVB->Unlock();

    if (FAILED(m_pDev->CreateIndexBuffer(sizeof(indices), 0, D3DFMT_INDEX32,
        D3DPOOL_MANAGED, &m_pIB, nullptr)))
        return false;
    m_pIB->Lock(0, 0, &p, 0); memcpy(p, indices, sizeof(indices)); m_pIB->Unlock();

    return true;
}

bool UnitManager::CreateMarkerMesh()
{
    // Flat unit-radius quad on XZ plane (y=0.05 to avoid z-fight with terrain)
    const float s = 1.0f;
    MarkerVertex verts[4] =
    {
        { -s, 0.05f, -s, 0, 0 },
        { +s, 0.05f, -s, 1, 0 },
        { -s, 0.05f, +s, 0, 1 },
        { +s, 0.05f, +s, 1, 1 },
    };

    if (FAILED(m_pDev->CreateVertexBuffer(sizeof(verts), 0,
        D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_MANAGED, &m_pMarkerVB, nullptr)))
        return false;
    void* p;
    m_pMarkerVB->Lock(0, 0, &p, 0); memcpy(p, verts, sizeof(verts)); m_pMarkerVB->Unlock();
    return true;
}

void UnitManager::AddMarker(const D3DXVECTOR3& pos, float life /*= 1.2f*/)
{
    Marker m;
    m.pos     = pos;
    m.life    = life;
    m.maxLife = life;
    m_markers.push_back(m);
}

Unit& UnitManager::Add(const D3DXVECTOR3& pos)
{
    Unit u;
    u.position  = pos;
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

    // Update markers, remove expired
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

        // Highlight / hit flash timers
        if (u.highlightTimer > 0) u.highlightTimer -= dt;
        if (u.hitFlashTimer > 0)  u.hitFlashTimer  -= dt;

        // Attack logic: chase target, attack in range
        bool attacking = false;
        if (u.attackTarget >= 0 && u.attackTarget < (int)m_units.size())
        {
            Unit& t = m_units[u.attackTarget];
            if (!t.alive)
            {
                u.attackTarget = -1;
            }
            else
            {
                D3DXVECTOR3 toT = t.position - u.position;
                toT.y = 0;
                float dist = D3DXVec3Length(&toT);

                if (dist > u.attackRange)
                {
                    // Chase: update targetPos to enemy position
                    u.targetPos = t.position;
                }
                else
                {
                    // In range: stop, face target, attack
                    u.targetPos = u.position;
                    attacking = true;
                    if (dist > 0.01f)
                    {
                        D3DXVECTOR3 dir = toT / dist;
                        u.yaw = atan2f(dir.x, dir.z);
                    }

                    // Refresh target's highlight so red ring stays while being attacked
                    if (t.highlightTimer < 1.0f) t.highlightTimer = 1.0f;

                    u.attackTimer -= dt;
                    if (u.attackTimer <= 0)
                    {
                        t.hp -= u.attackDamage;
                        t.hitFlashTimer = 0.15f;  // hit feedback
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
            // Movement toward targetPos
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
                else           u.position += dir * step;
            }
            else
            {
                u.moving = false;
            }
        }
    }
}

int UnitManager::PickUnit(const D3DXVECTOR3& rayOrigin, const D3DXVECTOR3& rayDir) const
{
    // Unit AABB: [pos.x-0.5, pos.x+0.5] x [pos.y, pos.y+1] x [pos.z-0.5, pos.z+0.5]
    int bestIdx = -1;
    float bestT = FLT_MAX;

    for (size_t i = 0; i < m_units.size(); i++)
    {
        const Unit& u = m_units[i];
        if (!u.alive) continue;
        D3DXVECTOR3 mn(u.position.x - 0.5f, u.position.y,        u.position.z - 0.5f);
        D3DXVECTOR3 mx(u.position.x + 0.5f, u.position.y + 1.0f, u.position.z + 0.5f);

        // Slab method
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
    if (index >= 0 && index < (int)m_units.size())
        m_units[index].selected = sel;
}

int UnitManager::SelectedCount() const
{
    int c = 0;
    for (const auto& u : m_units) if (u.selected) c++;
    return c;
}

void UnitManager::MoveSelectedTo(const D3DXVECTOR3& pos)
{
    // Spread destinations a bit so units don't overlap
    int selCount = SelectedCount();
    if (selCount == 0) return;

    int idx = 0;
    float spread = (selCount > 1) ? 0.8f : 0.0f;
    for (auto& u : m_units)
    {
        if (!u.selected || !u.alive) continue;
        float angle = (selCount > 1) ? (6.2831853f * idx / selCount) : 0;
        float off   = spread * sqrtf((float)idx);
        u.targetPos = D3DXVECTOR3(pos.x + cosf(angle) * off,
                                  pos.y,
                                  pos.z + sinf(angle) * off);
        u.attackTarget = -1;  // cancel attack when moving
        idx++;
    }
}

void UnitManager::AttackCommand(int targetIdx)
{
    if (targetIdx < 0 || targetIdx >= (int)m_units.size()) return;
    if (!m_units[targetIdx].alive) return;

    // Red ring on target for visual feedback (will be refreshed while attack continues)
    m_units[targetIdx].highlightTimer = 2.0f;

    for (auto& u : m_units)
    {
        if (!u.selected || !u.alive) continue;
        u.attackTarget = targetIdx;
        // targetPos updated in Update()
    }
}

void UnitManager::Render()
{
    if (!m_pDev || !m_pVB || !m_pIB || m_units.empty()) return;

    m_pDev->SetRenderState(D3DRS_ZENABLE,          TRUE);
    m_pDev->SetRenderState(D3DRS_ZWRITEENABLE,     TRUE);
    m_pDev->SetRenderState(D3DRS_LIGHTING,         FALSE);
    m_pDev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_CCW);
    m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pDev->SetRenderState(D3DRS_FILLMODE,         D3DFILL_SOLID);

    m_pDev->SetTexture(0, nullptr);
    m_pDev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    m_pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

    m_pDev->SetStreamSource(0, m_pVB, 0, sizeof(CubeVertex));
    m_pDev->SetIndices(m_pIB);
    m_pDev->SetFVF(UNIT_FVF);

    for (const auto& u : m_units)
    {
        if (!u.alive) continue;

        // World: rotate around Y by yaw, then translate to position
        D3DXMATRIX matRot, matTrans, matWorld;
        D3DXMatrixRotationY(&matRot, u.yaw);
        D3DXMatrixTranslation(&matTrans, u.position.x, u.position.y, u.position.z);
        matWorld = matRot * matTrans;
        m_pDev->SetTransform(D3DTS_WORLD, &matWorld);

        // Tint: selected > team color
        DWORD tint;
        if (u.selected)        tint = D3DCOLOR_ARGB(255, 255, 230, 80);
        else if (u.team == 0)  tint = D3DCOLOR_ARGB(255, 200, 80,  80);
        else                   tint = D3DCOLOR_ARGB(255, 80, 120, 240);

        // Hit flash: blend toward white based on remaining timer
        if (u.hitFlashTimer > 0)
        {
            float f = u.hitFlashTimer / 0.15f;
            if (f > 1) f = 1;
            BYTE r = (BYTE)((tint >> 16) & 0xFF);
            BYTE g = (BYTE)((tint >> 8)  & 0xFF);
            BYTE b = (BYTE)((tint >> 0)  & 0xFF);
            r = (BYTE)(r + (int)((255 - r) * f));
            g = (BYTE)(g + (int)((255 - g) * f));
            b = (BYTE)(b + (int)((255 - b) * f));
            tint = D3DCOLOR_ARGB(255, r, g, b);
        }
        m_pDev->SetRenderState(D3DRS_TEXTUREFACTOR, tint);
        m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);

        m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
            m_vertexCount, 0, m_indexCount / 3);
    }

    // Switching to shader-based draws: disable fixed-function fog
    m_pDev->SetRenderState(D3DRS_FOGENABLE, FALSE);

    // Read current view/proj (Map3D set them earlier) -- shared by rings & markers
    D3DXMATRIX view, proj;
    m_pDev->GetTransform(D3DTS_VIEW, &view);
    m_pDev->GetTransform(D3DTS_PROJECTION, &proj);
    D3DXMATRIX viewProj = view * proj;

    // --- Selection rings (green = selected, red = attack target) ---
    if (m_pMarkerVB && m_pSelectionEffect)
    {
        bool anySelected = false;
        bool anyTargeted = false;
        for (const auto& u : m_units)
        {
            if (!u.alive) continue;
            if (u.selected)            anySelected = true;
            if (u.highlightTimer > 0)  anyTargeted = true;
        }

        if (anySelected || anyTargeted)
        {
            m_pDev->SetStreamSource(0, m_pMarkerVB, 0, sizeof(MarkerVertex));
            m_pDev->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

            m_pSelectionEffect->SetTechnique("Tech_Selection");
            m_pSelectionEffect->SetFloat("time", m_time);

            UINT passes = 0;
            m_pSelectionEffect->Begin(&passes, 0);
            m_pSelectionEffect->BeginPass(0);

            const float ringScale = 0.8f;

            D3DXVECTOR4 green(0.25f, 1.0f, 0.3f, 1.0f);
            D3DXVECTOR4 red  (1.0f,  0.25f, 0.25f, 1.0f);

            for (const auto& u : m_units)
            {
                if (!u.alive) continue;

                bool wantGreen = u.selected;
                bool wantRed   = (u.highlightTimer > 0);
                if (!wantGreen && !wantRed) continue;

                D3DXMATRIX s, trans, w;
                D3DXMatrixScaling(&s, ringScale, 1.0f, ringScale);
                D3DXMatrixTranslation(&trans, u.position.x, u.position.y, u.position.z);
                w = s * trans;
                D3DXMATRIX wvp = w * viewProj;
                m_pSelectionEffect->SetMatrix("matWVP", &wvp);

                // Green ring (selected)
                if (wantGreen)
                {
                    m_pSelectionEffect->SetVector("ringColor", &green);
                    m_pSelectionEffect->CommitChanges();
                    m_pDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                }
                // Red ring (attack target)
                if (wantRed)
                {
                    m_pSelectionEffect->SetVector("ringColor", &red);
                    m_pSelectionEffect->CommitChanges();
                    m_pDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                }
            }

            m_pSelectionEffect->EndPass();
            m_pSelectionEffect->End();
        }
    }

    // --- Markers (shader-animated click feedback) ---
    if (!m_markers.empty() && m_pMarkerVB && m_pMarkerEffect)
    {

        m_pDev->SetStreamSource(0, m_pMarkerVB, 0, sizeof(MarkerVertex));
        m_pDev->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

        m_pMarkerEffect->SetTechnique("Tech_Marker");
        D3DXVECTOR4 col(1.0f, 0.2f, 0.2f, 1.0f);
        m_pMarkerEffect->SetVector("markerColor", &col);

        UINT passes = 0;
        m_pMarkerEffect->Begin(&passes, 0);
        m_pMarkerEffect->BeginPass(0);

        for (const auto& mk : m_markers)
        {
            float t  = mk.life / mk.maxLife;      // 1 -> 0
            float sz = 1.0f + (1.0f - t) * 1.5f;  // grow 1.0 -> 2.5 units radius

            D3DXMATRIX s, trans, w;
            D3DXMatrixScaling(&s, sz, 1.0f, sz);
            D3DXMatrixTranslation(&trans, mk.pos.x, mk.pos.y, mk.pos.z);
            w = s * trans;

            D3DXMATRIX wvp = w * viewProj;
            m_pMarkerEffect->SetMatrix("matWVP", &wvp);
            m_pMarkerEffect->SetFloat("progress", t);
            m_pMarkerEffect->CommitChanges();

            m_pDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        }

        m_pMarkerEffect->EndPass();
        m_pMarkerEffect->End();
    }

    // Restore
    D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
    m_pDev->SetTransform(D3DTS_WORLD, &identity);
    m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_pDev->SetRenderState(D3DRS_ZENABLE, FALSE);
}

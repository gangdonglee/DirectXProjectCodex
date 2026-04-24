#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>

class Map3D;

struct Unit
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 targetPos;
    float       yaw;
    float       hp;
    float       hpMax;
    bool        selected;
    bool        moving;
    bool        alive;
    int         team;            // 0 = player, 1 = enemy
    int         attackTarget;    // index of target unit, -1 = none
    float       attackTimer;
    float       attackRange;
    float       attackDamage;
    float       attackInterval;
    float       highlightTimer;   // red ring when targeted
    float       hitFlashTimer;    // white flash when taking damage

    Unit()
        : position(0, 0, 0), targetPos(0, 0, 0),
          yaw(0), hp(100), hpMax(100),
          selected(false), moving(false), alive(true),
          team(0), attackTarget(-1),
          attackTimer(0), attackRange(2.0f),
          attackDamage(15.0f), attackInterval(1.0f),
          highlightTimer(0), hitFlashTimer(0) {}
};

class UnitManager
{
public:
    UnitManager();
    ~UnitManager();

    bool Init(IDirect3DDevice9* dev,
              const char* unitShaderPath      = "unit.fx",
              const char* markerShaderPath    = "marker.fx",
              const char* selectionShaderPath = "selection.fx");
    void Shutdown();

    void Update(float dt);
    void Render();

    Unit& Add(const D3DXVECTOR3& pos);
    void  Clear();

    size_t Count() const                 { return m_units.size(); }
    Unit&  Get(size_t i)                 { return m_units[i]; }
    const Unit& Get(size_t i) const      { return m_units[i]; }
    std::vector<Unit>& GetAll()          { return m_units; }

    void SetMoveSpeed(float s)           { m_moveSpeed = s; }
    float GetMoveSpeed() const           { return m_moveSpeed; }

    // Pick first unit hit by ray (AABB test). Returns index or -1.
    int  PickUnit(const D3DXVECTOR3& rayOrigin, const D3DXVECTOR3& rayDir) const;

    void ClearSelection();
    void SetSelected(int index, bool sel);
    int  SelectedCount() const;
    void MoveSelectedTo(const D3DXVECTOR3& pos);
    void AttackCommand(int targetIdx);

    // Click marker (visual feedback for move orders)
    void AddMarker(const D3DXVECTOR3& pos, float life = 1.2f);

private:
    struct CubeVertex   { float x, y, z, nx, ny, nz; DWORD color; };
    struct MarkerVertex { float x, y, z; float u, v; };
    struct Marker       { D3DXVECTOR3 pos; float life, maxLife; };

    bool CreateCubeMesh();
    bool CreateMarkerMesh();

    IDirect3DDevice9*       m_pDev;
    IDirect3DVertexBuffer9* m_pVB;
    IDirect3DIndexBuffer9*  m_pIB;
    IDirect3DVertexBuffer9* m_pMarkerVB;
    ID3DXEffect*            m_pUnitEffect;
    ID3DXEffect*            m_pMarkerEffect;
    ID3DXEffect*            m_pSelectionEffect;
    float                   m_time;

    int  m_vertexCount;
    int  m_indexCount;
    std::vector<Unit>   m_units;
    std::vector<Marker> m_markers;
    float m_moveSpeed;
};

#pragma once
#include <d3d11.h>
#include "MathTypes.h"
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

    bool Init(ID3D11Device* dev, ID3D11DeviceContext* ctx,
              const char* unitShaderPath      = "unit.fx",
              const char* markerShaderPath    = "marker.fx",
              const char* selectionShaderPath = "selection.fx");
    void Shutdown();

    void Update(float dt);
    void Render(const Map3D& cam);

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

    ID3D11Device*          m_pDev;
    ID3D11DeviceContext*   m_pCtx;
    ID3D11Buffer*          m_pVB;
    ID3D11Buffer*          m_pIB;
    ID3D11Buffer*          m_pMarkerVB;
    ID3D11VertexShader*    m_pUnitVS;
    ID3D11PixelShader*     m_pUnitPS;
    ID3D11VertexShader*    m_pMarkerVS;
    ID3D11PixelShader*     m_pMarkerPS;
    ID3D11InputLayout*     m_pUnitLayout;
    ID3D11InputLayout*     m_pMarkerLayout;
    ID3D11Buffer*          m_pCB;
    ID3D11RasterizerState* m_pSolidRS;
    ID3D11BlendState*      m_pAlphaBlend;
    ID3D11DepthStencilState* m_pDepthOn;
    float                   m_time;

    int  m_vertexCount;
    int  m_indexCount;
    std::vector<Unit>   m_units;
    std::vector<Marker> m_markers;
    float m_moveSpeed;
};

#pragma once
#include <d3d11.h>
#include "MathTypes.h"

class HealthBarRenderer
{
public:
    HealthBarRenderer();
    ~HealthBarRenderer();

    bool Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width, int height, const char* shaderPath);
    void Shutdown();
    void Render(float x0, float y0, float x1, float y1, float progress);

private:
    struct QuadVertex { float x, y, u, v; };
    struct HealthBarCB
    {
        D3DXVECTOR4 screenParams;
        D3DXVECTOR4 data;
    };

    bool CompileShaders(const char* shaderPath);

    ID3D11Device*            m_pDev;
    ID3D11DeviceContext*     m_pCtx;
    ID3D11VertexShader*      m_pVS;
    ID3D11PixelShader*       m_pPS;
    ID3D11InputLayout*       m_pLayout;
    ID3D11Buffer*            m_pVB;
    ID3D11Buffer*            m_pCB;
    ID3D11BlendState*        m_pAlphaBlend;
    ID3D11DepthStencilState* m_pDepthOff;
    int                      m_width;
    int                      m_height;
};

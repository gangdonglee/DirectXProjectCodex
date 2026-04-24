#include "DX9Device.h"
#include <stdexcept>

DX9Device::DX9Device()
    : m_pD3D(nullptr), m_pDevice(nullptr), m_width(0), m_height(0)
{
}

DX9Device::~DX9Device()
{
    Shutdown();
}

bool DX9Device::Init(HWND hwnd, int width, int height, bool windowed)
{
    m_width  = width;
    m_height = height;

    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_pD3D)
        return false;

    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth        = width;
    pp.BackBufferHeight       = height;
    pp.BackBufferFormat       = windowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    pp.BackBufferCount        = 1;
    pp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow          = hwnd;
    pp.Windowed               = windowed ? TRUE : FALSE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    HRESULT hr = m_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp,
        &m_pDevice
    );

    if (FAILED(hr))
    {
        // HAL failed, retry with software vertex processing
        hr = m_pD3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,
            &m_pDevice
        );
    }

    return SUCCEEDED(hr);
}

void DX9Device::Shutdown()
{
    if (m_pDevice) { m_pDevice->Release(); m_pDevice = nullptr; }
    if (m_pD3D)    { m_pD3D->Release();    m_pD3D    = nullptr; }
}

void DX9Device::BeginFrame()
{
    if (!m_pDevice) return;
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     D3DCOLOR_XRGB(0, 0, 128), 1.0f, 0);
    m_pDevice->BeginScene();
}

void DX9Device::EndFrame()
{
    if (!m_pDevice) return;
    m_pDevice->EndScene();
}

void DX9Device::Present()
{
    if (!m_pDevice) return;
    m_pDevice->Present(nullptr, nullptr, nullptr, nullptr);
}

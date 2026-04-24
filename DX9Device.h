#pragma once
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

class DX9Device
{
public:
    DX9Device();
    ~DX9Device();

    bool Init(HWND hwnd, int width, int height, bool windowed = true);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Present();

    IDirect3DDevice9* GetDevice() const { return m_pDevice; }
    bool IsReady() const { return m_pDevice != nullptr; }

private:
    IDirect3D9*       m_pD3D;
    IDirect3DDevice9* m_pDevice;
    int               m_width;
    int               m_height;
};

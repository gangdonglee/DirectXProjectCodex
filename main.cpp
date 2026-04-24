#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "App.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    App app;
    if (!app.Init(hInst, nCmdShow))
        return -1;
    return app.Run();
}

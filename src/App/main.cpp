#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "App.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    App app;
    if (!app.Init(hInst, nCmdShow))
        return -1;
    return app.Run();
}

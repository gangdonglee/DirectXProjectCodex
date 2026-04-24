@echo off
setlocal

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo [HARNESS] Release build
call "%SCRIPT_DIR%build.bat"
if errorlevel 1 exit /b 1

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    set VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe
)

set MSBUILD=
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
        set MSBUILD=%%i
        goto :found_msbuild
    )
)

:found_msbuild
if not defined MSBUILD (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe
    )
)

if defined MSBUILD (
    echo.
    echo [HARNESS] Debug build
    "%MSBUILD%" "%SCRIPT_DIR%FontLib.sln" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
    if errorlevel 1 exit /b 1
) else (
    echo [WARN] MSBuild not found; skipped Debug build.
)

where rg >nul 2>nul
if not errorlevel 1 (
    echo.
    echo [HARNESS] DX9 reference scan
    rg -n "d3d9|d3dx9|IDirect3DDevice9|ID3DX|imgui_impl_dx9|DX9Device" -S . -g "!.git" -g "!bin" -g "!obj" -g "!.vs" -g "!lib/imgui/imstb_truetype.h" -g "!dev_harness.bat"
    if errorlevel 1 echo [OK] No active DX9 references found.
) else (
    echo [WARN] rg not found; skipped DX9 scan.
)

echo.
echo [HARNESS] Git status
git status --short --branch

echo.
echo [OK] Harness completed.
endlocal

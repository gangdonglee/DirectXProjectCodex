# FontLib - DX9 Font Rendering Library

## Project Overview

DirectX 9 + D3DXFont + .fx shader + ImGui based multi-text rendering library.
Windows 10, Visual Studio 2022 Community, MSVC cl.exe, DirectX SDK June 2010.
Supports dual build: VS2022 default toolset AND v140 (VS2015) + SDK 10.0.19041.0.

## File Structure

```
FontLib/
  main.cpp            - WinMain entry (14 lines, creates App)
  App.h / App.cpp     - Window, ImGui, background, main loop, UI (effect controls)
  DX9Device.h / .cpp  - D3D9 device wrapper (HAL/SW fallback, begin/end/present)
  TextRenderer.h/.cpp - Multi-text renderer (per-entry RT + shader, 5 effects)
  shader.fx           - PS-only: Simple/Outline/Glow/OutlineGlow/Dissolve
  shader_ansi.fx      - Same as shader.fx: ASCII + CRLF + no BOM + no comments
  noise.png           - Noise texture for Dissolve effect
  phase.fx            - Dissolve/Phase effect reference file (not used at runtime)
  TextOutlineGlow.fx  - Text outline+glow reference file (not used at runtime)
  build.bat           - MSVC v140 build script (vcvarsall + DXSDK_DIR, auto-kill app.exe)
  lib/imgui/          - ImGui v1.92.7 source + DX9/Win32 backends
  bin/                - Output (app.exe + shader.fx + noise.png copy)
  DEVLOG.md           - This file
```

## Architecture

### Rendering Pipeline (per frame)
```
1. DX9Device::BeginFrame()      - Clear backbuffer (blue D3DCOLOR_XRGB(0,0,128))
2. App::DrawBackground()        - Checkerboard texture (D3DFVF_XYZRHW, wrap tiling)
3. TextRenderer::Render()       - Per-entry: RT draw -> shader composite
   For each TextParams entry:
     a. RenderEntryToRT(p, font)      - Switch RT, clear alpha=0, draw white text
        (optional: D3DXSaveTextureToFileA capture when m_captureRT=true)
     b. CalcTextRect(p, font)         - DT_CALCRECT + outline padding (clamped)
     c. DrawEntryWithShader(p, bounds) - D3DFVF_XYZRHW quad, PS-only shader pass
        - effectMode selects technique: Simple/Outline/Glow/OutlineGlow/Dissolve
     d. DrawDebugBorder(bounds)        - Red LINESTRIP (if enabled)
   StateBlock: Capture before loop, Apply+Release after
4. ImGui::Render()              - UI overlay
5. DX9Device::Present()
```

### Shader Pipeline (shader.fx) -- PS-only, no vertex shader
- **D3DFVF_XYZRHW** pre-transformed vertices, VertexShader = NULL in all techniques
- **TextRender** technique (ps_2_0): `tex * g_TextColor` simple pass
- **TextRenderOutline** technique (ps_3_0):
  1. Enclosed detection: 2 axes (H, V), 2 samples per direction
     encMask = 1.0 - step(0.5, encMin) * step(center.a, 0.5) -- no early return
  2. Dilation: 8 directions x 3 distance steps, all tex2D inline (no helper functions)
  3. smoothstep(0.15, 0.6, maxAlpha) * encMask for crisp outline edge
  4. Smooth compositing: outline behind (outA*(1-txtA)), text on top (txtA)
- **TextRenderGlow** technique (ps_3_0):
  1. 11x11 gaussian blur kernel for glow alpha
  2. Glow behind + Simple text on top compositing (same as Simple+Glow layer)
- **TextRenderOutlineGlow** technique (ps_3_0):
  1. Glow (11x11 gaussian) + Outline dilation combined in one pass
  2. Composite: glow behind, outline+text on top
- **TextRenderDissolve** technique (ps_3_0):
  1. Noise texture sampling (noise.png via NoiseSampler, WRAP addressing)
  2. smoothstep-based alpha with glowing edge (custom inline, no helper fn)
  3. Edge color/intensity for dissolve border glow
- Uniforms: g_TextColor, g_OutlineColor, g_TexelSize, g_OutlineSize, g_TextTexture
- Glow uniforms: g_GlowColor, g_GlowWidth, g_GlowIntensity
- Dissolve uniforms: g_Progress, g_EdgeWidth, g_NoiseScale, g_EdgeColor, g_EdgeIntensity
- Dissolve texture: g_NoiseTex + NoiseSampler
- **VS2015/D3DX effect runtime compatibility**:
  - No helper functions with sampler params (D3DX can't inline)
  - No static const arrays (fxc OK but D3DX runtime fails)
  - No early return in PS (D3DX runtime fails)
  - No bool/&&/ternary chains (v140 ICE crash)
  - All tex2D calls inline in PS function body

### TextEffect enum
```cpp
enum TextEffect {
    EFFECT_SIMPLE = 0,    // TextRender technique
    EFFECT_OUTLINE,       // TextRenderOutline technique
    EFFECT_GLOW,          // TextRenderGlow technique
    EFFECT_OUTLINE_GLOW,  // TextRenderOutlineGlow technique
    EFFECT_DISSOLVE,      // TextRenderDissolve technique
    EFFECT_COUNT
};
```

### TextRenderer Key Details
- `vector<TextParams>` entries, each rendered independently to shared RT
- `vector<FontEntry>` font cache by size (GetOrCreateFont, D3DXCreateFontW, CLEARTYPE_QUALITY)
- RT is screen-sized (A8R8G8B8, RENDERTARGET), cleared per entry (alpha=0)
- White text drawn to RT, shader handles coloring + outline/glow/dissolve
- **QuadVertex**: {x, y, z, rhw, u, v} with D3DFVF_XYZRHW | D3DFVF_TEX1
- **Outline size auto-clamp**: effectiveOutline = min(outlineSize, fontSize * 0.4)
- **Noise texture**: loaded from noise.png via D3DXCreateTextureFromFileA
- **RT Capture debug**: CaptureRT() -> saves rt_capture_N.png per entry via D3DXSaveTextureToFileA
- **DrawEntryWithShader**: switch(effectMode) selects technique + sets corresponding uniforms
- StateBlock (D3DSBT_ALL) protects caller device state
- Depth disabled (ZEnable=false) + DS detached during RT write

### TextParams struct
```cpp
char  text[1024]       // UTF-8 (converted via MultiByteToWideChar)
float color[4]         // RGBA (default: 1,1,0,1)
float outlineColor[4]  // RGBA (default: 0,0,0,1)
int   posX, posY       // Screen position
int   fontSize         // D3DXFont height
int   outlineSize(=2)  // Outline radius (clamped to fontSize*0.4 at render)
bool  outlineEnabled(=true)
int   effectMode(=EFFECT_OUTLINE)  // TextEffect enum

// Glow params
float glowColor[4]     // RGBA (default: 0.4, 0.7, 1.0, 1.0)
float glowWidth(=5.0)
float glowIntensity(=0.8)

// Dissolve params
float dissolveProgress(=0.0)
float edgeWidth(=0.1)
float noiseScale[2](={1,1})
float edgeColor[4]     // RGBA (default: 1.0, 0.8, 0.2, 1.0)
float edgeIntensity(=1.5)

Match(text,x,y,fontSize,color) // Exact match with float epsilon
```

### TextRenderer API
```cpp
Add(text, x, y, fontSize, color)           -> TextParams&
Remove(index) / Remove(text,x,y,sz,color)  -> void / bool
Find(text, x, y, fontSize, color)          -> TextParams*
FindAll(text)                              -> vector<TextParams*>
Get(index) / Count() / Clear()
SetDebugBorder(bool) / GetDebugBorder()
CaptureRT()                                // Save RT as PNG next frame
```

### App Structure
- `DX9Device m_device` - D3D9 init (800x600 windowed, D3DFMT_D24S8 depth)
- `TextRenderer m_textRenderer` - Init with "shader.fx"
- `IDirect3DTexture9* m_pBgTex` - 256x256 procedural checkerboard (blue tones)
- ImGui fonts: malgun.ttf(KR) + msjh.ttc(CN) + leelawui.ttf(TH) merged
- Demo texts: "Hello, FontLib!"(yellow 32pt outline), "Multi-text!"(cyan 48pt outline), "Small text"(white 16pt simple)
- UI: TreeNode per entry with Effect combo (Simple/Outline/Glow/Outline+Glow/Dissolve)
  - Outline mode: Outline Size slider + Outline Color
  - Glow mode: Glow Color + Glow Width + Glow Intensity
  - Dissolve mode: Progress + Edge Width + Edge Color + Edge Intensity
  - Add/Remove, Debug Border checkbox, Capture RT button

### DX9Device
- HAL with HW vertex processing, fallback to SW vertex processing
- Windowed: D3DFMT_UNKNOWN backbuffer, D3DFMT_D24S8 auto depth stencil
- VSync: D3DPRESENT_INTERVAL_ONE

## Build System

### Current: v140 (VS2015) toolset
```bat
vcvarsall.bat amd64 10.0.19041.0 -vcvars_ver=14.0
cl /nologo /EHsc /W3 /O2 /MD
    /I"%DXSDK_INC%" /I"lib\imgui" /I"lib\imgui\backends"
    main.cpp App.cpp DX9Device.cpp TextRenderer.cpp [imgui sources]
    /link /LIBPATH:"%DXSDK_LIB%" d3d9.lib d3dx9.lib user32.lib gdi32.lib
    /subsystem:windows /out:bin\app.exe
```
- **No /utf-8 flag** -- v140 doesn't support it. No Korean comments in source files.
- **No chcp 65001** -- v140 cl.exe crashes with UTF-8 codepage (D8000 error storm)
- build.bat auto-kills app.exe before build (taskkill)
- Copies shader.fx + noise.png to bin/ after build
- Portable: `%~dp0`, vswhere auto-detect + fallback paths

### shader_ansi.fx generation
- ASCII encoding + CRLF line endings + no BOM + no comments
- Generated via PowerShell: strip comments from shader.fx, write with ASCIIEncoding
- **Needs regeneration** after Glow/Dissolve additions

## Known Issues & Gotchas

| Issue | Solution |
|-------|----------|
| .bat Korean broken (BOM) | No BOM, English comments only in .bat |
| ImGui crash 2768 | No manual Build(), init fonts after ImGui_ImplDX9_Init |
| C2059 UTF-8 source | `/utf-8` flag (VS2022 only, not v140) |
| LNK1104 locked exe | build.bat auto-kills app.exe |
| `small` MSVC macro conflict | Rename variable |
| strncpy deprecation | strncpy_s + _TRUNCATE |
| v140 + chcp 65001 = D8000 | Don't use chcp 65001 with v140 cl.exe |
| v140 + /utf-8 = D8000 | Don't use /utf-8 with v140. Remove Korean comments. |
| v140 + SDK 10.0.26100 = _mm_loadu_si64 | Use SDK 10.0.19041.0 instead |
| v140 ICE on bool&&ternary | Use min()/step() math instead of conditionals |
| D3DX effect: helper fn with sampler | Inline all tex2D in PS body, no helper functions |
| D3DX effect: static const array | Use local float2 variables instead |
| D3DX effect: early return in PS | Use float mask (encMask) multiplied into result |
| Outline engulfs small text | effectiveOutline = min(outlineSize, fontSize*0.4) |
| Outline fills glyph holes | Enclosed detection via step() math in shader |
| Outline smeared/bleeding | smoothstep(0.15, 0.6) sharpens edge |
| C4819 warning (Korean in .cpp) | Use English comments only (changed DX9Device.cpp) |
| Outline not showing in ImGui | effectMode directly selects technique, no outlineEnabled gate |
| Glow shows only glow, no text | PS_Glow composites: glow behind + Simple text on top |
| VB Lock crash after RT recreate | RT Release → RT Create → VB Lock = crash (memcpy). Must do: RT Release → VB Lock → RT Create |

## Device Reset / Resolution Change

### D3DPOOL_DEFAULT RT + VB Lock ordering issue
When recreating D3DPOOL_DEFAULT render targets during resolution change, the order matters:
- **WRONG:** RT Release → RT Create → VB Lock (crash in memcpy — device internal state conflict)
- **CORRECT:** RT Release → VB Lock (mesh update) → RT Create

### Reset() handling checklist
| Resource | Pool | Action |
|----------|------|--------|
| m_pRTSurf | DEFAULT (from RT) | Release before m_pRT |
| m_pRT | DEFAULT (RENDERTARGET) | Release → recreate after Reset |
| m_pEffect | internal | OnLostDevice() / OnResetDevice() |
| m_fonts (ID3DXFont) | internal | OnLostDevice() / OnResetDevice() |
| m_pNoiseTex | MANAGED | automatic (no action) |
| m_pBgTex | MANAGED | automatic (no action) |

**Key constraint:** RENDERTARGET requires D3DPOOL_DEFAULT (MANAGED not allowed).
If any DEFAULT resource is not released before Reset(), Reset() fails (D3DERR_INVALIDCALL) and all subsequent D3D calls behave unpredictably.

## Shader embedding options (discussed, not implemented)

User asked about loading .fx without external file. Three approaches:
1. **C++ string embedding** -- shader source as `const char*`, use `D3DXCreateEffect` (works but 300+ line string)
2. **fxc precompile** -- .fxo bytecode as `BYTE[]` array (requires per-technique compile)
3. **Windows resource (.rc)** -- .fx as RCDATA, load via `FindResource`/`LoadResource` (cleanest, .fx stays separate in source but embedded in exe)

## Current State (2026-04-03)

All features complete and building with v140 (VS2015) toolset:
DX9 device, window, D3DXFont, 5-technique shader pipeline (PS-only, XYZRHW),
ImGui with effect selection UI, multilingual, multi-text container, Find/FindAll,
debug border, background texture, StateBlock, RT capture debug, noise texture,
portable build, shader_ansi.fx for external use (needs regen).

5 shader techniques: Simple, Outline, Glow (with text), OutlineGlow, Dissolve.

## Next Steps

- Regenerate shader_ansi.fx with Glow/Dissolve content
- Shader embedding (resource or string) to eliminate external .fx dependency
- Font face selection per entry
- Text shadow (offset + blur)
- FPS counter
- Device lost/reset handling
- Text animation (fade, scroll)

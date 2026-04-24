# DirectX Project Devlog

Current renderer: DirectX 11 + Dear ImGui.

## Architecture

- `DX11Device` owns device, immediate context, swap chain, back buffer RTV, and depth buffer.
- `Map3D` renders the terrain grid with D3D11 vertex/index buffers and embedded HLSL.
- `UnitManager` renders units, selection rings, attack target rings, click markers, hit flashes, and unit simulation.
- `SDFRenderer` draws overlay text directly through DX11.
- `SDFAtlas` builds a CPU SDF atlas with stb_truetype and uploads it as an `ID3D11ShaderResourceView`.
- Dear ImGui uses the Win32 + DX11 backends.

## Build

Use:

```bat
build.bat
```

The Visual Studio project builds `Release|x64` to `bin\app.exe`.

Linked graphics libraries:

- `d3d11.lib`
- `dxgi.lib`
- `d3dcompiler.lib`

## Runtime Notes

- Left click selects player units.
- Drag selects multiple player units.
- Right click ground moves selected units.
- Right click enemy issues attack command.
- Selected units show a green ring.
- Attack targets show a red ring.
- Hit units flash toward white briefly on each damage event.

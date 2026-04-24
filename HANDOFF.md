# Agent Handoff

다른 코드 에이전트와 공유하는 작업 인수인계 문서. 작업 끝낼 때마다 이 파일을 갱신한다.

- **마지막 갱신**: 2026-04-24 (Codex)
- **브랜치**: `main` (origin/main과 동기)
- **최근 기준 커밋**: `7b6da9c` Port rendering system to DirectX 11 / 이후 변경은 `git log` 참조

---

## 현재 진행 중인 작업

- 현재 미커밋 작업 없음이 목표. 변경이 생기면 `dev_harness.bat` 검증 후 바로 커밋한다.

---

## 최근 완료된 작업

### A. 셰이더 외부 `.fx` 파일 분리 리팩터링

기존엔 `Map3D.cpp` / `UnitManager.cpp`에 `kTerrainShader` / `kUnitShader`가 raw string literal로 박혀 있었음. 디스크의 `terrain.fx` / `unit.fx` / `marker.fx` / `selection.fx`를 `D3DCompileFromFile`로 컴파일하도록 교체.

| 파일 | 변경 |
|---|---|
| [Map3D.cpp](Map3D.cpp) | 인라인 셰이더 제거. `CompileShader` → `CompileShaderFile`로 교체 (UTF-8 경로 → wide 변환 헬퍼 추가). `terrainShaderPath` 인자 활성화. 진입점 `VSMain/PSMain` → `VS_Terrain/PS_Terrain` |
| [UnitManager.cpp](UnitManager.cpp) | 동일 패턴. `m_pSelectionPS` 신설 — 이전엔 marker의 `RingPS`가 selection ring도 담당했으나 분리. `hitFlashTimer`를 `data.y`에 실어 셰이더로 전달 |
| [UnitManager.h](UnitManager.h) | `ID3D11PixelShader* m_pSelectionPS` 추가 |
| [terrain.fx](terrain.fx) | DX9 (`technique/pass`, `tex2D`, `vs_3_0`) → DX11 (`cbuffer`, `Texture2D`/`SamplerState`, `SV_POSITION`/`SV_TARGET`) |
| [unit.fx](unit.fx) | 동일한 DX11 문법 변환. fog 로직 제거 |
| [marker.fx](marker.fx) | DX11 변환. `UnitCB` 공유 (`data.y` = progress) |
| [selection.fx](selection.fx) | marker에서 분리. **PS만 정의** (VS는 marker VS 재사용) |

### B. 셀렉션 링·클릭 마커 컬링 버그 픽스

**증상**: DX11 포팅 직후부터 `selection.fx` / `marker.fx`만 화면에 안 나옴 (큐브·지형은 정상).

**원인**: `m_pMarkerVB`의 수평 쿼드 정점 순서로 인해 첫 삼각형의 노멀이 `(0, -4, 0)` (아래 방향). RTS 카메라(위에서 내려다봄) 기준 백페이스가 됨. DX9 원본은 패스마다 `CullMode = None`이었으나 DX11 포팅 시 모든 드로우가 `m_pSolidRS` (CULL_BACK)을 공유하게 됨.

**수정** ([UnitManager.cpp](UnitManager.cpp), [UnitManager.h](UnitManager.h)):
- `ID3D11RasterizerState* m_pNoCullRS` 멤버 추가, `D3D11_CULL_NONE`으로 생성
- `Render()`에서 셀렉션·마커 루프 직전에 `RSSetState(m_pNoCullRS)`, 종료 후 `m_pSolidRS`로 원복
- Shutdown에 ReleaseCOM 추가

### 알려진 주의점

- **`selection.fx`에 `VS_Selection`이 없다** — 의도된 설계. `UnitManager::Init`에서 `PS_Selection`만 컴파일하고 marker VS / 입력 레이아웃을 공유. "VS 빠졌다"고 추가하면 안 됨.
- **`build.bat`이 모든 `.fx`를 `bin/`으로 복사함** ([build.bat:46-53](build.bat#L46-L53)). 런타임 CWD 기준으로 `D3DCompileFromFile`이 동작하므로 VS 디버그 시엔 프로젝트 루트, `bin/app.exe` 직접 실행 시엔 `bin/` 둘 다 OK.
- **hitFlash가 이중 적용 가능성**: [UnitManager.cpp:444-451](UnitManager.cpp#L444-L451)에서 CPU 측 흰색 블렌드 + [unit.fx:36](unit.fx#L36)에서 `lerp(lit, white, data.y)`. 의도된 동작인지 확인 필요. 아직 시각 검증 안 함.
- 줄바꿈 경고 (LF→CRLF) 다수 — Windows 환경 정상 동작, 무시 가능.

---

## 다음에 할 일 (제안)

0. **TODO 관리** — 기능 로드맵은 [TODO.md](TODO.md)에 정리됨. 신규 기능은 해당 파일에 먼저 추가/정렬.
1. **실행 검증** — 빌드는 통과 (`bin/app.exe` 14:35+). 셀렉션 링 / 클릭 마커가 화면에 보이는지, 큐브 · 지형 회귀 없는지 확인.
2. **hitFlash 이중 적용 결정** — CPU 쪽 또는 셰이더 쪽 한 군데로 일원화. 시각 보고 결정.
3. **단일 커밋 정리** — 두 작업(A, B)을 한 커밋으로 묶을지 분리할지 사용자에게 확인. 후보 메시지: `Externalize shaders to .fx files and fix marker/selection culling`.

---

## 프로젝트 구조 메모

- **빌드**: `build.bat` → `Release|x64` → `bin/app.exe`. VS 솔루션 `FontLib.sln`.
- **렌더러**: DX11 + Dear ImGui (Win32+DX11 백엔드).
- **소유 관계**: `App`이 `DX11Device` / `Map3D` / `UnitManager` / `TextRenderer` / `SDFRenderer` / `SDFAtlas` 를 모두 보유. 1024×720.
- **링크 라이브러리**: `d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`.
- 자세한 아키텍처는 [DEVLOG.md](DEVLOG.md) 참조.

---

## 인수인계 규칙

- 의미 있는 작업을 마치면 이 문서의 **마지막 갱신** / **진행 중 작업** / **다음에 할 일** 섹션을 갱신한다.
- 변경사항이 생기면 검증 후 바로 커밋한다.
- 반복 검증은 [dev_harness.bat](dev_harness.bat)를 사용한다. 현재 하네스는 Release 빌드, Debug 빌드, DX9 참조 스캔, git 상태 확인을 수행한다.
- 커밋되지 않은 WIP의 의도, 비명시적 설계 결정, "건드리면 안 되는 것"을 우선적으로 적는다. (코드/git log로 알 수 있는 내용은 굳이 중복 기재하지 않는다.)
- 작업자 식별을 위해 갱신 시 모델/에이전트 이름을 적어둔다.

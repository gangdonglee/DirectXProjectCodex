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

### C. 유닛 충돌 회피

- `Unit`에 `collisionRadius` 추가. 기본값은 `0.45f`.
- `MoveSelectedTo()`는 클릭 지점 주변에 격자 formation 목적지를 배치한다. 기존 나선형 분산보다 목적지 근처 겹침이 적다.
- `UnitManager::ApplySeparation(float dt)`가 매 프레임 alive 유닛끼리 XZ 평면에서 최소 거리(`radiusA + radiusB`)를 유지하도록 부드럽게 밀어낸다.
- 공격 중인 유닛은 적 중심이 아니라 적 주변 원형 슬롯을 `targetPos`로 사용한다. 여러 유닛이 같은 적을 공격할 때 뭉침과 떨림을 줄이기 위한 처리다.
- 이동 도착 판정은 `collisionRadius` 기반 여유 반경을 사용한다. separation이 살짝 밀어낸 유닛을 다시 같은 점으로 끌어당기는 현상을 줄인다.
- dead unit은 separation에서 제외한다.

### D. HealthBar / SDFRenderer DX11 직접 렌더링

- `HealthBarRenderer` 추가. `healthbar.fx`의 `VS_HealthBar` / `PS_HealthBar`를 `D3DCompileFromFile`로 컴파일하고 screen-space quad를 DX11 dynamic vertex buffer로 그린다.
- `SDFRenderer`는 ImGui draw list 의존을 제거했다. `sdf.fx`의 `VS_SDF`와 4개 PS(`Simple/Outline/Glow/OutlineGlow`)를 컴파일하고 SDF atlas SRV를 직접 샘플링한다.
- `App::RenderHealthBars()`는 ImGui 대신 `m_healthBarRenderer.Render(...)`를 호출한다.
- `TextRenderer`와 배경/드래그 박스/UI는 아직 ImGui 경로가 남아 있다. 이번 작업 범위는 `HealthBar`와 `SDFRenderer`만이다.

### 알려진 주의점

- **`selection.fx`에 `VS_Selection`이 없다** — 의도된 설계. `UnitManager::Init`에서 `PS_Selection`만 컴파일하고 marker VS / 입력 레이아웃을 공유. "VS 빠졌다"고 추가하면 안 됨.
- **`build.bat`이 모든 `.fx`를 `bin/`으로 복사함** ([build.bat:46-53](build.bat#L46-L53)). 런타임 CWD 기준으로 `D3DCompileFromFile`이 동작하므로 VS 디버그 시엔 프로젝트 루트, `bin/app.exe` 직접 실행 시엔 `bin/` 둘 다 OK.
- **셰이더 디버깅 조건**: Debug 빌드에서만 D3D11 debug layer와 HLSL debug info가 활성화된다. `Map3D.cpp` / `UnitManager.cpp`의 `D3DCompileFromFile`은 `_DEBUG`에서 `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`을 사용한다. Visual Studio Graphics Diagnostics/PIX로 셰이더 단계 디버깅 시 Debug 구성으로 실행해야 한다.
- **hitFlash 소유권**: 셰이더가 담당한다. CPU 렌더 경로는 기본 tint만 `UnitCB.color`로 보내고, `UnitCB.data.y`에 flash 강도를 넘겨 [unit.fx](unit.fx)의 `PS_Unit`에서 흰색 보간을 수행한다.
- 줄바꿈 경고 (LF→CRLF) 다수 — Windows 환경 정상 동작, 무시 가능.

---

## 다음에 할 일 (제안)

0. **TODO 관리** — 기능 로드맵은 [TODO.md](TODO.md)에 정리됨. 신규 기능은 해당 파일에 먼저 추가/정렬.
1. **실행 검증** — 빌드는 통과 (`bin/app.exe` 14:35+). 셀렉션 링 / 클릭 마커가 화면에 보이는지, 큐브 · 지형 회귀 없는지 확인.
2. **hitFlash 시각 검증** — 셰이더 단일 적용으로 과하거나 약하지 않은지 실행 화면에서 확인.
3. **유닛 충돌 회피 시각 검증** — 단체 이동 후 목적지 근처에서 과한 떨림/밀림이 없는지 확인.
4. **SDF/HealthBar 시각 검증** — ImGui 제거 후 SDF 텍스트와 체력바가 이전 위치/색/알파로 보이는지 확인.

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
---

## 2026-04-24 Update - TextRenderer DX11 Direct Path

- `TextRenderer` no longer includes ImGui or uses `ImDrawList`.
- It owns an internal `SDFAtlas` and `SDFRenderer`, then maps legacy `TextParams` into SDF draw entries each frame.
- `shader.fx` is now a DX11-compatible include shim over `sdf.fx`, so the existing `App::Init(..., ""shader.fx"")` call remains valid.
- Legacy dissolve/combined text controls are mapped to the closest SDF modes for now. Full dissolve text needs a later dedicated DX11 text shader if the effect is still required.
- Remaining ImGui usage is debug/editor UI plus background and drag-box overlays, not `TextRenderer`.

# TODO

## 0. Current Stabilization

- [ ] Verify runtime rendering after DX11 shader reconnection.
  - Check terrain shader from `terrain.fx`.
  - Check unit cube shader from `unit.fx`.
  - Check selected-unit ring from `selection.fx`.
  - Check click/attack marker from `marker.fx`.
- [x] Decide hit flash ownership.
  - Shader owns hit flash through `unit.fx` and `UnitCB.data.y`.
  - CPU render path now sends base tint only.
- [ ] Reconnect remaining overlay shaders if needed.
  - `healthbar.fx`
  - `shader.fx`
  - `sdf.fx`

## 1. Combat And Unit Behavior

- [x] Unit collision avoidance near destination.
  - Selected move commands assign a grid formation around the clicked point.
  - Alive units run a lightweight XZ separation pass using `collisionRadius`.
  - Attackers reserve spread-out slots around their target instead of all chasing the target center.
  - Dead units are ignored by separation.
- [ ] Projectiles for attacks.
  - Spawn projectile when ranged attack fires.
  - Move projectile from attacker to target over time.
  - Apply damage on hit or keep current instant damage and use projectile as visual only.
  - Add projectile render path and impact feedback.
- [ ] Unit types.
  - Add melee/ranged unit type enum.
  - Move stats into per-type data: hp, speed, range, damage, interval, projectile speed.
  - Update spawn/test setup to create mixed unit types.
  - Reflect type in selection/attack behavior and optional color/model differences.

## 2. World Building

- [ ] Heightmap terrain.
  - Add height data to `Map3D`.
  - Generate or load heightmap.
  - Update grid vertices, normals, picking, ground intersection, and unit movement height.
  - Keep fog and terrain shader compatible.
- [ ] Buildings.
  - Add static structure data and placement.
  - Render fixed building meshes or placeholder boxes.
  - Add collision/picking bounds.
  - Block unit movement/target placement where appropriate.
- [ ] Fog of war.
  - Define vision radius per unit/building.
  - Maintain explored/visible grid.
  - Render hidden/explored overlay.
  - Hide or dim enemy units/buildings outside vision.

## 3. Controls And Selection

- [ ] Control groups.
  - `Ctrl+1` through `Ctrl+9`: save current selection.
  - `1` through `9`: recall saved selection.
  - Handle dead units being removed from recalled groups.
  - Optional: double-tap number focuses camera on group.
- [ ] Double-click same-type selection.
  - Detect double-click timing and screen distance.
  - Select all visible/alive player units of the clicked unit's type.
  - Respect shift-add behavior if needed.

## Suggested Order

1. Stabilize DX11 shader reconnection and visual regressions.
2. Unit collision avoidance.
3. Unit types.
4. Projectiles.
5. Control groups and double-click selection.
6. Heightmap.
7. Buildings.
8. Fog of war.

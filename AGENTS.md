# AGENTS.md — raycast-2d Engineering & Design Standards

This document defines the architectural, performance, memory-layout, visual, and verification standards for `raycast-2d`. All future changes must adhere to these principles.

---

## 1. Core Philosophy: Extreme Performance & Minimalism

- **Zero External Dependencies**: Pure C++17 + native OS APIs only (`user32`, `gdi32`, `winmm`, `ws2_32` on Windows; POSIX `termios`/`ioctl` on Linux/macOS). Never pull in heavy third-party frameworks (SDL, GLFW, OpenGL, ImGui, etc.). Keep the standalone Windows release binary compact (~200–250 KB) with zero external DLLs.
- **No Unnecessary Elevation**: On Windows, render via the native Win32 `DIBSection` glyph-atlas window rather than `conhost.exe` hacks. No UAC administrator manifest or elevation wrappers.
- **Pure 2D Raycast / 2.5D Core**: Keep rendering anchored in column-based 2D DDA raycasting (`src/camera.cpp`) with 2.5D heightfield floors/walls, Y-shearing pitch, analytic spherical sky domes, and Z-buffered billboard props/particles.
- **Strict Platform Decoupling**: Core simulation, rendering, and UI (`Camera`, `WorldMap`, `Screen`, `HudUi`, `ConsoleUi`) must remain platform-agnostic behind `Platform` (`include/platform.h`).

---

## 2. Object Byte Layout & Memory Locality Standards

Every hot-path struct must be designed for cache-line efficiency, minimal padding, and O(1) flat indexing, guarded by compile-time `static_assert` checks:

| Type | Target Size | Required Layout & Locality Invariants |
| --- | --- | --- |
| `Screen::Cell` | **2 B** | `{char glyph; unsigned char color;}` — interleaved for single fetch & 16-bit dirty compare. |
| `Texel` | **2 B** | `{char glyph; unsigned char color;}` — interleaved so one cache line yields both glyph and color. |
| `Tile` | **16 B** | `double floor` (exact physics/test precision) + `float wallHeight` + direct 1-based `uint8_t wall, surface, side, pad`. Zero tail padding; 4 tiles per 64 B cache line; eliminates secondary texture-ID indirection in hot loops. |
| `WorldTexture` | **28 B** | Pure POD descriptor (zero per-texture heap pointers). All texel frames across all textures live in one contiguous `WorldMap::texelAtlas_` arena. |
| `WorldProp` | **32 B** | `float` transform/animation fields + direct `uint8_t textureSlot`. 2 props per 64 B cache line. |
| `Emitter` | **32 B** | `float` bounds/speed + `uint16_t count` + `uint8_t kind, color1, color2`. 2 emitters per 64 B cache line. |
| `Swarm` | **20 B** | Compact `float` + `uint16_t`. |
| `WorldMap::flatGrid_` | **1 B/cell** | Single flat `std::vector<uint8_t>` (`y * gridWidth_ + x`). Never use `std::map` or `std::vector<std::string>` on hot render/physics paths. |
| `Camera::depthBuffer_` | **4 B/pixel** | Column-major `std::vector<float>` (50% smaller than `double`). |

### Memory & Allocation Rules
- **Zero Per-Frame Heap Allocations** in `Camera::render`, `HudUi::draw`, `ConsoleUi::draw`, and `Platform::present`. Reuse member scratch buffers (`depthBuffer_`, `invRowDist_`, `visibleProps_`, `prevCells_`).
- **L1-Hot Depth Initialization**: Initialize each column's `depthCol` inside the parallel column worker immediately before DDA stepping—never do a separate single-threaded cold-RAM `assign()` pass before rendering.
- **Single-Arena Textures**: Pack all atlas frames into `WorldMap::texelAtlas_` to avoid scattered per-texture heap blocks.

---

## 3. Algorithmic & Performance Standards

### Frame Pacing & Timing
- **Locked 500 FPS (`2000 µs/frame`)**: Use a high-precision hybrid pacer (`timeBeginPeriod(1)` on Windows + `sleep_for(1ms)` when remaining budget `> 1.35 ms` + `YieldProcessor()` spin for the final sub-millisecond). Reset deadline on overrun to prevent catch-up bursts.
- **Framerate-Independent Physics**: Never apply an artificial minimum floor to `dt` (`std::clamp(rawDt, 0.0, 0.05)`).
- **Network Decoupling**: Keep LAN pose/input packet transmission throttled to ~60 Hz regardless of render FPS.

### Raycasting & Sky Algorithms (`src/camera.cpp`)
- **Multi-Threaded Column Stripes**: Use the persistent `ColumnPool` when `screenWidth >= 256`, aligning stripe boundaries to **32 columns (64 B = 1 cache line)** to prevent false sharing. Zero thread creation per frame.
- **Invariant Hoisting**:
  - Per frame: precompute `invRowDist_[y]` (`H` divisions instead of `W * H`), `SkyFrame` (meteor & cosmic eye state).
  - Per column: compute `SkyColumn` (azimuth, celestial bounding-box cull flags, Milky Way `riftCenter`, storm cloud height) and track exact skyline occlusion `skyStop` to skip occluded sky pixels without touching `depthCol`.
  - Per span: hoist `frameData(tex, time)`, wall horizontal `texX`, and step vertical `v += vStep` linearly.
- **Inner-Loop Fast Math**:
  - Replace inner-loop `std::sin` with the L1-resident 256-entry `SinLut`; replace sky `std::atan` with `fastAtanPos`; prefer power-of-two bitmasks over integer modulo.
  - Use fast inline fractional wrap (`wrapTex`) instead of CRT `std::floor`, and take a zero-wrap fast path for non-flowing props (`(texture.flags & 1u) == 0u`).
  - Back-plane frustum sphere cull entire particle `Emitter`s before iterating their particles.

### Native Windows Display Pipeline (`src/platform_terminal_windows.cpp`)
- **Pre-Baked 1-Bit Glyph Masks**: Bake printable ASCII `32..126` once at startup into a compact L1-resident bitmask table (`1 byte` per glyph row, ~570 B total).
- **Persistent `DIBSection` + 16-Bit Dirty Skip**: Compare packed 16-bit `(glyph, color)` cells against `prevCells_` and skip unchanged cells completely.
- **Unrolled Branchless Glyph Blitters**: Maintain specialized unrolled fast paths for common glyph widths (`gw == 4` exact 16-byte row, `gw == 3`, `gw == 5`) with empty-row fast fills.
- **Minimal Present**: Use `CS_OWNDC` (no per-frame `GetDC`/`ReleaseDC`) and `BitBlt` only the tight 2D dirty bounding rectangle `[dirtyMinX..dirtyMaxX] × [dirtyMinY..dirtyMaxY]`.

---

## 4. Visual, Map & UI Design Standards

- **Opt-In Extensions & Full Backward Compatibility**: New engine directives (`sky eldritch|starry`, `fog`, `wave`, `monolith`, `morph`, `emitter`, `prop ... float`, `anim`, `colors`, optional `tile` `wallHeight`/`side`) must remain opt-in so legacy maps (`foundry`, `skyway`, `lantern_court`, `star_well`) render bit-for-bit identically.
- **Deterministic Analytic Animation**: All terrain motion, texture animation, sky motion, and particle emitters must be pure functions of `animationTime` so LAN multiplayer stays synchronized with zero particle packets.
- **Window-Proportional Scalable UI (`src/hud_ui.cpp`)**:
  - Scale all HUD panels, compass, radar minimap, crosshair, and title/pause/realm/controls overlays cleanly across 4 resolution tiers using the compact 2-byte/glyph 3×5 bitmap font.
  - Keep HUD toggleable (`H`), radar toggleable (`Tab`), realm switcher accessible live in-game (`M` / `Esc`), console on `` ` ``/`~`, and fullscreen on `F11`/`Alt+Enter`.

---

## 5. Build, Verification & Regression Gates

Every change affecting engine code, assets, or build configuration must pass all gates before committing:

1. **Clean Release Build (0 Warnings)**:
   ```powershell
   cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release
   cmake --build out/play --config Release --target raycast-2d render_scene_test render_bench
   ```
   MSVC compiles with `/W4 /permissive-`; GCC/Clang with `-Wall -Wextra -Wpedantic`. Zero warnings permitted.
2. **Scene & Feature Regression Test**:
   ```powershell
   .\out\play\Release\render_scene_test.exe
   ```
   Must exit `0` (validates `rlyeh`, `star_well`, `lantern_court`, `foundry`, `skyway`, physics, pitch/turn, avatar occlusion, dynamic terrain/morphs, and starry sky).
3. **Deterministic Render Benchmark**:
   ```powershell
   .\out\play\Release\render_bench.exe
   .\out\play\Release\render_bench.exe --rlyeh
   ```
   - Legacy FNV64 hashes must remain bit-exact: `foundry.map` = `c65796860b0d5cdc`, `skyway.map` = `e212b4d5e81c06df`.
   - R'lyeh CPU render time at `760×240` should stay well within the `2.0 ms` (500 FPS) frame budget (`< 1.0 ms` target).

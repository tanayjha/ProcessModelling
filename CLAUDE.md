# UMPNAP — Agent Orientation & Project Context

**Read this first.** It is the entry point for any session working on this repo
(including automated ones, e.g. answering Telegram queries). It does **not**
duplicate the README or the equations doc — it links them and adds the code map,
extension recipes, dev/push workflow, and current state/roadmap.

## What this project is

UMPNAP is a C++17 / Qt6 multi-domain process-network analysis & dynamic
simulation platform (think a digital-plant simulator for hydraulic, pneumatic,
steam, electrical, instrument and control systems). A validated single-phase
**hydraulic nodal solver** is the working core; pneumatic runs on the same
solver with a gas fluid; electrical and two-phase steam are wireable library
symbols pending dedicated solvers.

- **Features, build/run, "how to add a component", validation** → `README.md`
- **Governing equations of every model + the network solve** → `docs/EQUATIONS.md`
- **Original design rationale / plan** → `docs/superpowers/specs/2026-06-06-umpnap-design.md`, `docs/superpowers/plans/2026-06-06-umpnap.md`
- **Annotated screenshots** → `docs/umpnap_*.png`

## Build, test, run (essential commands)

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j4
ctest --test-dir build            # 11 engine tests; MUST stay green
./build/umpnap                    # GUI;  ./build/umpnap examples/control.umpnap --run
```
Toolchain: Apple clang (C++17), CMake ≥ 3.16, Qt6 (`brew install qt`). The engine
builds and tests **without Qt**; only the GUI needs it.

## Architecture invariants (do not break)

- **The GUI never solves.** It only edits a `Network`; `SolverManager` consumes it.
- **The engine (`core` + `solver` + `components`) is Qt-free** and links as the
  static lib `umpnap_engine`, independently testable. The GUI lib `umpnap_gui`
  links the engine + Qt. `core` must not depend on `gui`/`solver`.
- **Solver = nodal pressure formulation**, Newton-Raphson + dense LU, mass
  conservation per node; transient = explicit-Euler tank inventory between
  steady re-solves. Bidirectional flow via signed `Q`.
- **Per-port medium typing** gates connections: `Port.medium`
  (Process/Liquid/Gas/Steam/Electrical/Signal); `effectiveMedium()` resolves
  Process→fluid medium. Only equal media may connect.
- **`NodeGraph` includes only solving participants** (branches, tanks,
  boundaries, junctions, headers). Non-solving (steam/electrical/instrument)
  components must never create nodes (else singular Jacobian).
- **Plugin component model:** everything is a `Component` created from a
  `ComponentDef` in the `ComponentRegistry`. Palette, property editor, datasheet,
  save/load and solver are all driven off the registry — adding a type touches
  one place (plus its branch law if it flows).

## Code map

Engine (`src/`):
| File | Responsibility |
|---|---|
| `core/Component.h` | `Component`, `Port`, `Medium`, `Domain`, `effectiveMedium`/`fluidMedium` |
| `core/Network.{h,cpp}` | component+connection store, `validate()`, domain/medium helpers, `componentByName` |
| `core/ComponentRegistry.{h,cpp}` | **all library type definitions** (ports, params, tag prefix, default fluid) |
| `core/FluidLibrary.{h,cpp}` | fluids (liquids incl. boron/Gd poisons, gases), `liquids()/gases()/isGas()` |
| `core/Results.{h,cpp}` | time-series store + CSV |
| `core/Project.{h,cpp}` | hand-rolled JSON save/load (`.umpnap`); serializes params, config, curves |
| `core/CurveFit.{h,cpp}` | least-squares polyfit (pump head curve) |
| `solver/LinAlg.{h,cpp}` | dense LU with partial pivoting |
| `solver/NodeGraph.{h,cpp}` | collapse ports→nodes, pin boundary/tank pressures (+ tank cover-gas) |
| `solver/HydraulicSolver.{h,cpp}` | Newton-Raphson steady solve; records node P, branch Q, derived signals |
| `solver/SolverManager.{h,cpp}` | `runSteady`, `runTransient`, `stepTransient` (one cycle) |
| `solver/ControlSolver.{h,cpp}` | PI controllers → valve position each cycle (tag-linked) |
| `solver/Validation.{h,cpp}` | analytical series+parallel check (matches solver ~5e-10) |
| `solver/{ISolver.h,StubSolvers.h}` | solver interface + gas/thermal/electrical stubs |
| `components/hydraulic/BranchLaw.{h,cpp}` | **branch physics** (pipe/valve/orifice/pump/HX/filter + pneumatic aliases), `isBranch`, `isTankType` |

GUI (`src/gui/`): `main.cpp` (CLI: `[project.umpnap] [--run]`), `MainWindow`
(menus/toolbar/sim controls/search/View menu), `DiagramScene`+`DiagramItems`
(canvas, P&ID symbols, wiring, medium validation, runtime overlays, flow arrows),
`PaletteDock` (library-grouped tree), `PropertyEditor`, `PlantData` (datasheet +
curve editor), `HierarchyDock`, `TrendDock`+`TrendWidget` (hover/window/range/PNG),
`SimController` (Run/Pause/Step/Speed/Reset/Snapshot), `Equations` (per-type text).

Tests: `tests/*.cpp` (one executable each, registered in `tests/CMakeLists.txt`,
tiny `check.h` harness). Examples + generators: `examples/make_*.cpp` build
`.umpnap` files; verification/screenshot tools: `tools/*.cpp`.

## Extension recipes

- **New component type:** add a `registerDef({...})` in
  `ComponentRegistry.cpp` (type, domain, tag prefix, ports w/ media, params,
  default fluid). If it carries flow, add its law to `evalBranch` and list it in
  `isBranch` (`BranchLaw.cpp`); add a symbol case in `DiagramItems.cpp drawSymbol`
  and a group in `PaletteDock.cpp libraryOf`. Palette/editor/datasheet/save/load
  pick it up automatically.
- **New fluid:** add to `kTable` in `FluidLibrary.cpp` (set the `gas` flag).
- **New solver (e.g. electrical):** implement `ISolver`, register in
  `SolverManager`, gate participation in `NodeGraph` if it's a separate network.
- **Headless UI verification (use this to "see" changes without a display):**
  ```bash
  QT_QPA_PLATFORM=offscreen ./build/umpnap_screenshot <project> <out.png> [simSteps]
  QT_QPA_PLATFORM=offscreen ./build/umpnap_datasheet  <project> <out.png> [tab]
  QT_QPA_PLATFORM=offscreen ./build/umpnap_palette    <out.png>
  QT_QPA_PLATFORM=offscreen ./build/umpnap_trendcheck <project> <out.png>
  ```
  Then Read the PNG to confirm the render.

## Dev & push workflow (for automated sessions)

1. Make the change; keep every edit traceable to the request (no drive-by churn;
   match existing style).
2. **Build + run ctest; both must pass before any push.** For GUI changes also
   render via the offscreen tools above and inspect the PNG.
3. Commit with a clear `feat:/fix:/docs:` message (no Claude attribution).
4. Push: `git push origin main` — on this machine `origin` is preconfigured with
   a PAT in `.git/config` (do **not** print or commit the token; it lives only in
   local git config, never in tracked files).
5. Keep `CLAUDE.md`, `README.md`, `docs/EQUATIONS.md` updated when behavior or
   architecture changes.

**For the Telegram-bot loop:** decide per query — if it's a question, answer
concisely from the code/docs (no push); if it asks for an improvement/fix, make
the change, build+test+verify, push, and report what changed + the commit. Never
push unverified code.

## Current state & roadmap

**Working now** (see README for the full feature list, `git log` for history —
20 commits as of HEAD `6e915f4`): validated hydraulic + pneumatic nodal solver;
interactive sim engine (run/pause/step/speed/reset/snapshot/initial-conditions);
runtime monitoring (live values + flow arrows); P&ID symbols + tags; per-port
medium validation; multi-fluid vessels (cover gas); library across hydraulic/
air-gas/steam/electrical/instrument-control; named valve characteristics; plant-
data datasheet ↔ property sync; PI control coupling; trends (hover/window/range/
export); deletable wires; multi-select; View menu; elevation incl. signed pipe
static head; library-grouped palette; tag search.

**Deferred / requested but not yet built** (priority order for improvement work):
1. Shell-and-tube **HX split** (separate shell/tube fluids & connections — needed
   for e.g. PHWR bleed-condenser cooling by process water).
2. **Instrument/actuator → valve/pump attachment** links (signal connections).
3. **Engineering-unit switching** for display: flow (m³/s, kg/s, t/h), level
   (mm/cm/m), pressure (bar/mbar/kPa/kg·cm⁻²).
4. **Wildcard component search** → results list → plant-data popup (current
   toolbar search only selects one).
5. **Node numbers on the mimic diagram** + node renaming (correlate with trends).
6. **Scalable components** (resize on canvas).
7. **Runtime-state save/load to file** (in-memory snapshots + IC files exist;
   add named on-disk runtime snapshots).
8. **Dedicated solvers:** electrical power-flow; two-phase/steam thermo.
9. **Library benchmark pass** vs leading simulators to complete parameter sets &
   reflect them in the equations.

When picking up an improvement request, check this list first; if it matches a
roadmap item, implement it and move it to "working".

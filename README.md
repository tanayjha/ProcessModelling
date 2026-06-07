# UMPNAP — Unified Multi-Domain Process Network Analysis Platform

A local, offline, Qt-based process-network analysis platform. You drag components
from a palette onto a P&ID canvas, wire them together, configure parameters and a
working fluid, then solve the network and trend the results.

This is **Phase 1** of the design proposal: a real, analytically-validated
single-phase **hydraulic** Newton-Raphson solver, behind an architecture built to
accept the gas / thermal / electrical domains later.

![UMPNAP canvas — moderator-style D2O loop](docs/umpnap_canvas.png)

*(The image above is a real render of the bundled `examples/loop.umpnap`: a closed
heavy-water loop — Tank → Pump → Pipe → Heat Exchanger → Valve → Tank.)*

## What works today

- **Drag-and-drop P&ID editor** (Qt `QGraphicsView`) with **standard P&ID symbols**
  per type (Boundary = circle, Pump = circle+impeller, Valve = bowtie, Tank =
  cylinder with live level, Orifice = plate, Pipe = flanged spool, HX = shell +
  serpentine, instruments = ISA bubbles). Place, drag to move, port-to-port wire,
  `Delete` to remove. Connection validation rejects inlet→inlet / outlet→outlet.
- **Editable P&ID tags** on every component (e.g. `P-101`, `FCV-203`), shown on the
  symbol and saved with the project. Build as many diagrams as you like with your
  own naming convention.
- **Plant Data datasheet** (`Edit ▸ Plant Data…`): one tab per component type, a
  table of your tagged instances against their design fields. Edit any cell —
  changes apply immediately and the next solve reflects them, so transient response
  tracks your plant data.
- **Detailed, equation-referenced models** with engineering design data: Pipe
  (Darcy-Weisbach + minor losses, ID/OD/roughness/tuning), Valve (IEC 60534 Kv with
  linear/equal-%/quick-open characteristic), Orifice (ISO 5167), Pump (head curve
  fitted from measured points or rated/shutoff datasheet, affinity laws),
  Heat Exchanger (shell-and-tube tube-side). Every law is heavily commented in
  `src/components/hydraulic/BranchLaw.cpp`; full write-up in
  [`docs/EQUATIONS.md`](EQUATIONS.md); summary shown live in the Properties dock.
- **Pump head–flow curve editor**: enter measured `(Q,H)` points; a least-squares
  quadratic (in-tree LU) drives the model.
- **Hydraulic solver**: nodal pressure formulation, Newton-Raphson with a dense LU
  linear solver, mass conservation at every node. Steady-state and transient
  (explicit-Euler tank-level integration).
- **Generic fluid property package**: Light Water, Heavy Water (D2O), Oil, Air,
  Helium, Nitrogen. The *same* component adapts to the selected fluid — no
  hardcoded fluid constants.
- **Expanded plugin library**: Boundary, Tank, Pipe, Valve, Orifice, Pump, Junction,
  Heat Exchanger (hydraulic) plus Transmitter, Actuator, Controller (instrument
  symbols; not yet solve-coupled). Palette, property editor, and datasheet are all
  generated from the registry.
- **Property editor**, **model-hierarchy browser**, **trend plots** (`QPainter`,
  per-series auto-scale), **CSV export**, and full **project save/load** (`.umpnap`).
- **In-app solver validation**: `Help ▸ Validate Solver` runs a series+parallel
  network with a closed-form answer and reports the error (matches to ~5e-10).

![Plant Data datasheet](docs/umpnap_plantdata.png)

### Dynamic simulation engine & runtime monitoring

A live simulation engine (Simulation toolbar/menu) with **Run, Pause/Freeze,
Single-Step, Fast-Forward (1–20×), Reset, Save/Restore Snapshot**, a running
clock, and capturable/loadable **Initial Conditions** (`File ▸ Save/Load Initial
Condition`). One tick advances exactly one solver cycle; Single-Step advances one
(for debugging); Freeze stops sim time while you inspect/edit. As it runs, **live
values appear on every symbol** (pressure, flow, level, head, valve %) and
**animated arrows show flow direction** on each connection. Trends grow in real
time. Components can be **cloned** (`Ctrl+D`); projects carry a schema version.

![Runtime monitoring — live values + flow arrows](docs/umpnap_runtime.png)

### Control loops

A `Controller` links by tag to a measured component and an output valve and runs
a discrete PI law each cycle (`u = Kp·err + (Kp/Ti)·∫err`, clamped to valve
travel, with anti-windup). `examples/control.umpnap` holds a tank level at a 5 m
setpoint by modulating its inlet valve.

### Expanded multi-domain library

Beyond the core hydraulic set: **Filter, Strainer, Header, PressurizedTank,
AirReceiver**, and pneumatic **Duct, Damper, Fan, Blower, Compressor** (which
solve on the nodal engine with Air); plus **electrical** (Grid, Generator,
Transformer, Busbar, Breaker, Cable, Motor, Load) and **instrument/control**
(Transmitter, Switch, RTD, Gauge, Actuator, Controller, Timer, Logic) as
configurable, tagged P&ID symbols. Each draws its standard glyph.

![Symbol gallery](docs/umpnap_gallery.png)

> **Scope / roadmap.** This is a working core of the full digital-plant vision,
> not the whole thing. Delivered: the simulation engine, runtime monitoring,
> snapshots/initial conditions, a validated hydraulic+pneumatic nodal solver,
> basic PID control, and a broad component library. **Not yet implemented:** a
> true electrical power-flow solver (electrical components are symbols/datasheets
> only), compressible-steam/two-phase thermodynamics, discrete logic/state-machine
> execution, and every component variant in the original spec. The architecture
> (common base, plugin registry, `ISolver` per domain) is built to extend into
> these without rework.

## What is stubbed (later phases)

Gas, thermal, and electrical solvers are **registered behind the same `ISolver`
interface but do not solve yet** — they report "not implemented". Coupled
gas–liquid (cover-gas) and two-phase/steam are not attempted. One correct,
validated hydraulic solver was prioritised over five unverified ones.

## Dependencies

- A C++17 compiler (tested with Apple clang 17)
- CMake ≥ 3.16
- Qt6 Widgets — only needed for the GUI. The engine + tests build without Qt.
  - macOS: `brew install qt`
  - RHEL/Bharat Linux: install the `qt6-qtbase`/`qt6-qtbase-devel` packages

No other third-party libraries. The linear algebra and JSON I/O are in-tree, and
the platform makes **no network calls** (fully offline).

## Build, test, run

```bash
# Configure (point CMake at Qt; on macOS use the brew prefix)
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"

# Build everything (engine, tests, GUI)
cmake --build build -j4

# Run the test suite (engine is Qt-free, so this works even without Qt)
ctest --test-dir build --output-on-failure

# Launch the GUI
./build/umpnap
```

If Qt6 is not found, CMake still builds the engine and tests (`ctest` passes);
only the `umpnap` GUI target is skipped.

### Using the app

1. Click a component type in the **Component Palette** (left), then click the
   canvas to place it.
2. Drag from one component's **port handle** (the colored dots) to another port to
   wire them. Green = inlet, red = outlet, blue = bidirectional.
3. Select a component to edit its parameters and **fluid** in the **Properties**
   dock (right).
4. `Run ▸ Run Steady` (or `Run ▸ Run Transient…`) to solve. Pick signals in the
   **Trends** dock (bottom) to plot pressure / flow / level / pump head / valve
   position. `File ▸ Export Results CSV…` to save.
5. `File ▸ Open…` `examples/loop.umpnap` to load the bundled example.

## Architecture

The engine is a Qt-free static library (`umpnap_engine`) so it is independently
testable; the GUI links it. The GUI never solves — it only edits a `Network`,
which the `SolverManager` consumes.

```
src/
  core/        Component, Network, ComponentRegistry, FluidLibrary,
               Results, Project (JSON)
  solver/      LinAlg (dense LU), NodeGraph, ISolver, HydraulicSolver,
               SolverManager, StubSolvers, Validation
  components/
    hydraulic/ BranchLaw (Darcy-Weisbach, orifice, valve, pump, HX laws)
  gui/         DiagramScene + items, Palette/Property/Hierarchy/Trend docks,
               MainWindow
tests/         10 CTest unit tests (LU, fluids, network, registry, branch laws,
               node graph, solver, validation, project round-trip)
examples/      loop.umpnap + the generator that produced it
```

### Solver in one paragraph

Unknowns are the pressures at the network's free nodes (junctions collapse to a
single node; boundaries and tanks pin their pressure). Each branch element gives a
flow `Q(ΔP)` and its derivative: pipes use Darcy-Weisbach with a laminar/Swamee-Jain
friction factor, orifices/valves/heat-exchangers use a quadratic resistance, pumps
use a `H = H0 − a·Q²` head curve. Mass conservation at each free node forms
`F(P)=0`, solved by Newton-Raphson (`J·ΔP = −F` via dense LU) with a backtracking
line search. Transient runs integrate tank levels and re-solve each step.

## How to add a component

1. Register a `ComponentDef` (type, domain, ports, parameter schema) in
   `registerHydraulicComponents()` (`src/core/ComponentRegistry.cpp`).
2. If it is a flow element, add its `Q(ΔP)` law to `evalBranch`
   (`src/components/hydraulic/BranchLaw.cpp`) and list it in `isBranch`.

The palette, property editor, hierarchy, save/load, and solver pick it up
automatically — no GUI changes needed.

## Validation

`Help ▸ Validate Solver` (and the `validation_test` CTest) build a series+parallel
orifice network whose flow and node pressures have a closed-form solution, and
compare. Latest run: **max relative error ≈ 5e-10**, Newton converged in 6
iterations.
```
Q: solver=0.0162478  analytic=0.0162478   (err 5.0e-10)
```

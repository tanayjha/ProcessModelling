# UMPNAP — Unified Multi-Domain Process Network Analysis Platform

**Design Spec — 2026-06-06**

## 1. Goal & Scope

Bring the UMPNAP design proposal into a runnable local reality. This first deliverable
implements **Phase 1 (single-phase hydraulic solver)** to real, validated depth, behind an
architecture that lets gas/thermal/electrical solvers slot in later without rework.

- **Stack:** C++17, Qt6 Widgets (Core/Gui/Widgets only), CMake.
- **No external math libraries.** A small dense LU (partial pivoting) linear solver is written
  in-tree; hydraulic networks here are tens–hundreds of unknowns, so dense is fine.
- **Trends** drawn with `QPainter` (no QtCharts dependency).
- **Only system dependency:** Qt6 (`brew install qt` on macOS; `qt6-*` packages on RHEL).
- **Offline:** no network calls anywhere. Honors "operate completely offline on RHEL/Bharat Linux".

### Out of scope for this pass (honest caveats)
- Gas, thermal, and electrical solvers ship as **interface stubs** (registered, but not solving).
  One correct hydraulic solver beats five broken ones.
- Coupled gas-liquid (moderator cover-gas) coupling is not solved; the hydraulic loop is.
- Two-phase / steam (Phase 5) is not attempted.
- "Drag and drop" is implemented as palette-click-to-place plus drag-on-canvas reposition and
  port-to-port wiring — the standard `QGraphicsScene` idiom.

## 2. Architecture

The spec's eight pieces map to code modules:

| Spec piece | Module | Responsibility |
|---|---|---|
| Qt graphical editor | `src/gui` | Canvas, palette, property editor, hierarchy, trends, menus |
| Project repository | `src/core/Project` | JSON save/load of full project (`.umpnap`) |
| Model manager | `src/core/Network` | In-memory components + connections; topology validation |
| Solver manager | `src/solver/SolverManager` | Assemble global system, drive Newton-Raphson, own timestep loop |
| Domain solvers | `src/solver/ISolver` + `HydraulicSolver` | Pluggable solvers; hydraulic implemented, others stubbed |
| Property package | `src/core/FluidLibrary` | Density/viscosity/cp/k for 6 fluids |
| Results database | `src/core/Results` | Time-series store keyed by component/node signal |
| Plotting/reporting | `src/gui/TrendWidget` + CSV export | Real-time + post-run trends; CSV report |

**Hard rule from the spec:** the GUI never solves equations. It only defines topology and
configuration. The GUI mutates a `Network`; the `SolverManager` consumes it.

### Module dependency direction
```
gui  ->  core  <-  solver  ->  components
              \________________/
```
`core` has no dependency on `gui` or `solver`. `solver` and `components` depend on `core`.
The engine (core + solver + components) builds as a static library `umpnap_engine` with **no Qt
dependency**, so it is independently testable via CTest. `gui` links Qt + the engine.

## 3. Data Model (`core`)

- **`Component`** — id, type name, domain, display position, parameter map (`name -> double`,
  plus a fluid selection), and a list of **ports**. Each port has a name and a role
  (`Inlet`/`Outlet`/`Bidirectional`).
- **`Connection`** — links `(componentA, portA)` to `(componentB, portB)`.
- **`Network`** — owns components and connections; provides lookup, add/remove, and
  `validate()` (every non-boundary port connected, no dangling, at least one boundary/tank to
  anchor pressure).
- **`Node`** — a solver-side concept: a maximal set of ports joined by connections (a junction
  collapses its ports to one node). Built by the `SolverManager` from the `Network`, not stored.

## 4. Component Library (`components/hydraulic`)

Plugin-based via a `ComponentRegistry`. Each type registers: type name, domain, a factory, a
parameter schema (name, unit, default, min/max), and a port layout. The palette and property
editor are generated from the registry — adding a component is one registered class.

Phase-1 hydraulic components and their branch law (flow `Q`, pressure drop `dP = P_in - P_out`):

| Component | Ports | Law |
|---|---|---|
| **Boundary** | 1 | Fixed pressure node (`P = P_set`). Anchors the system. |
| **Tank** | 1 (or 2) | Node pressure = `P_atm + rho*g*level`. `level` integrated transiently from net inflow / area. |
| **Pipe** | inlet, outlet | Darcy-Weisbach: `dP = K*Q*|Q|`, `K = f*(L/D)*rho/(2*A^2)`, friction `f` from laminar (64/Re) or Swamee-Jain turbulent correlation using fluid `rho`, `mu`. |
| **Valve** | inlet, outlet | `dP = (K_v / pos^2) * Q*|Q|`, `pos` = fractional opening (0–1); clamps near-closed. |
| **Orifice** | inlet, outlet | `dP = K_o * Q*|Q|`, `K_o` from discharge coeff, bore area, `rho`. |
| **Pump** | inlet, outlet | Head curve `dP = -rho*g*(H0 - a*Q^2)` (rise across pump). |
| **Junction** | N | Ideal node, zero pressure drop; merges connected ports into one solver node. |
| **HeatExchanger** | inlet, outlet | Hydraulic side only: fixed-resistance `dP = K_hx*Q*|Q|`. (Thermal side is Phase 2.) |

Every branch law that needs fluid properties pulls them from the component's selected fluid via
`FluidLibrary` — no hardcoded fluid constants. Switching fluid in the GUI re-evaluates the law.

## 5. Fluid Property Package (`core/FluidLibrary`)

Fluids: **Light Water, Heavy Water (D2O), Oil, Air, Helium, Nitrogen.** Each provides density,
dynamic viscosity, specific heat, thermal conductivity at a reference temperature (constant-property
model for Phase 1; temperature-dependence is a later hook). Values from standard engineering tables.
A component stores a fluid id; the registry default is Light Water.

## 6. Solver (`solver`)

**Formulation:** nodal. Unknowns = pressure at every solver node that is not pinned by a Boundary
or Tank. Each branch component contributes a flow `Q(dP)` and its derivative `dQ/d(dP)`.

**Residual:** mass conservation at each free node — sum of branch flows in/out = 0 (incompressible,
so volumetric balance). `F(P) = A^T Q(A P) = 0` where `A` is the node-branch incidence.

**Method:** Newton-Raphson.
1. Build node list from the `Network` (collapse junctions, merge connected ports).
2. Pin pressures from Boundary/Tank nodes.
3. Iterate: assemble residual `F` and Jacobian `J = dF/dP` (analytic where the law gives `dQ/dP`,
   numeric fallback otherwise), solve `J dP = -F` via dense LU with partial pivoting, update,
   damp if needed, until `||F|| < tol` or max iters.
4. Report convergence (iterations, residual) up to the GUI; expose non-convergence as an alarm.

**Transient:** outer loop integrates Tank `level` with explicit Euler (`level += dt*Qnet/area`),
re-pins tank pressures, re-runs the steady Newton solve each step, and records results. Steady-state
analysis is the special case of one step.

**SolverManager** owns the solver registry (`ISolver` per domain), picks hydraulic for hydraulic
components, runs the timestep loop, and writes node pressures + branch flows + component-derived
signals (pump head, valve position, tank level, heat duty placeholder) into `Results`.

**LinAlg:** in-tree dense matrix, `lu_factor` / `lu_solve` with partial pivoting. Unit-tested
against known systems.

## 7. Results & Trends

`Results` stores, per recorded signal (`component.signal` or `node.pressure`), a vector of
`(time, value)`. The GUI `TrendWidget` renders selected signals with `QPainter` — auto-scaled axes,
multiple series, legend. Trendable per the spec: Pressure, Flow, Tank Level, Pump Head, Valve
Position (Temperature/Inventory/Heat Duty exposed as signals but zero/placeholder until thermal).
CSV export writes the full results table.

## 8. GUI (`gui`)

- **MainWindow:** menu/toolbar (New, Open, Save, Run Steady, Run Transient, Validate, Export CSV),
  central canvas, dockable palette, property editor, hierarchy browser, trends, and a status/alarm
  line.
- **Canvas (`QGraphicsView`/`Scene`):** components are `QGraphicsItem`s with port handles; click a
  palette entry then click canvas to place; drag to reposition; drag port-to-port to connect;
  connection validation rejects illegal links (e.g. two outlets, cross-domain in this pass).
- **Property editor:** reflects the selected component's parameter schema from the registry; edits
  write back to the `Network`; includes the fluid selector.
- **Hierarchy browser:** tree of components by domain.
- **Save/restore** of the complete project via the JSON `Project` serializer.
- **Engineering units:** parameters carry units in their schema and are shown in the editor; values
  stored internally in SI.

## 9. Validation Strategy ("prove it's real")

Primary deliverable proving correctness: a **series + parallel pipe network** with a closed-form
analytical solution.
- **As a CTest unit test** (`tests/`): build the network in code, solve, assert node pressures and
  branch flows match the analytical result within tolerance.
- **In-app Validation menu item:** runs the same case and reports solver-vs-analytical max error in
  a dialog.

Additional tests: LinAlg LU against known systems; each component's branch law at a known operating
point; `Network::validate` rejects malformed topologies; Project JSON round-trips.

## 10. Repository Layout

```
umpnap/
  CMakeLists.txt              # top-level: engine lib, gui app, tests
  README.md                   # build + run instructions
  src/
    core/                     # Component, Port, Connection, Network, Project,
                              #   FluidLibrary, Results, ComponentRegistry
    solver/                   # ISolver, SolverManager, HydraulicSolver, LinAlg,
                              #   stub Gas/Thermal/Electrical solvers
    components/hydraulic/     # Boundary, Tank, Pipe, Valve, Orifice, Pump,
                              #   Junction, HeatExchanger
    gui/                      # MainWindow, Canvas, PaletteDock, PropertyEditor,
                              #   HierarchyDock, TrendWidget, ConnectionItem
  tests/                      # validation_pipe_network, linalg, components,
                              #   network_validate, project_roundtrip
  examples/                   # sample .umpnap project (small loop)
```

## 11. Build & Run

```
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
ctest --test-dir build        # runs validation + unit tests
./build/umpnap                # launches the GUI
```

## 12. Roadmap Hooks (later phases, not this pass)

- Phase 2 Thermal: `ThermalSolver` implements `ISolver`; HeatExchanger gains a thermal side;
  FluidLibrary already exposes cp/k.
- Phase 3 Gas: `GasSolver` (compressible nodal) behind the same interface; gas components register
  into the existing registry.
- Phase 4 Coupled: `SolverManager` already coordinates multiple `ISolver`s — add a coupling pass.

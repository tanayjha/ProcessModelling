# UMPNAP — Governing Equations of the Library Models

This document states the thermohydraulic equations behind each component library
model, the design parameters that drive them, and references. The same equations
appear, heavily commented, in the model source:
`src/components/hydraulic/BranchLaw.cpp`. The GUI also shows a short summary per
component in the **Properties** dock.

## Notation

| Symbol | Meaning | Unit |
|---|---|---|
| `ΔP` | pressure drop across element, `P_in − P_out` | Pa |
| `Q` | volumetric flow, inlet → outlet | m³/s |
| `ρ`, `μ` | fluid density, dynamic viscosity (from the property package) | kg/m³, Pa·s |
| `v` | mean velocity | m/s |
| `A` | flow area | m² |
| `g` | standard gravity = 9.80665 | m/s² |
| `Re` | Reynolds number `ρ v D / μ` | – |

Sign convention: `ΔP > 0 ⇒ Q > 0` (flow from inlet to outlet).

Most passive elements reduce to a **quadratic resistance** `ΔP = K · Q·|Q|`, so
`Q = sign(ΔP)·√(|ΔP|/K)`, with `dQ/dΔP = 1/(2√(K|ΔP|))`. A small linear core near
`ΔP = 0` keeps the Newton Jacobian finite. Only `K` differs between elements.

---

## Pipe — Darcy-Weisbach with minor losses

Design data: `length L`, inner diameter `ID`, outer diameter `OD` (wall info),
`roughness e`, lumped minor-loss coefficient `ΣK` (fittings), `tuning` factor.

```
A = π·ID²/4 ,   v = Q/A
ΔP = ( f·L/ID + ΣK ) · ρ v²/2 · tuning           ⇒   K = ( f·L/ID + ΣK )·ρ/(2A²)·tuning
```

Darcy friction factor `f`:
```
laminar  (Re < 2300):   f = 64 / Re                              (Hagen–Poiseuille)
turbulent:              f = 0.25 / [ log10( e/(3.7·ID) + 5.74/Re^0.9 ) ]²   (Swamee–Jain)
```
`f` depends on `Re(Q)`, so `K` is re-evaluated each Newton iteration.
**Refs:** Darcy-Weisbach equation; Colebrook–White; Swamee & Jain (1976); Crane TP-410 (minor-loss K).

## Valve — IEC 60534 flow coefficient with inherent characteristic

Design data: rated `Kv` [m³/h/bar^0.5], `characteristic` (0 linear, 1 equal-%,
2 quick-open), `rangeability R`, `position x ∈ [0,1]`.

```
installed coefficient:  Kv_eff = Kv · φ(x)
   φ = x            (linear)
   φ = R^(x−1)      (equal-percentage)
   φ = √x           (quick-opening)
sizing:   Q[m³/h] = Kv_eff · √( ΔP[bar] / SG ),  SG = ρ/ρ_water
```
In SI as a resistance: `K = (10⁵·ρ/1000)·(3600/Kv_eff)²`.
**Ref:** IEC 60534-2-1; ISA control-valve sizing.

## Orifice — ISO 5167 thin-plate metering

Design data: bore `d`, upstream pipe bore `D`, discharge coefficient `Cd`.

```
β = d/D ,   A_o = π d²/4
Q = ( Cd / √(1−β⁴) ) · A_o · √( 2 ΔP / ρ )        ⇒   K = (ρ/2)·(1−β⁴)/(Cd²·A_o²)
```
**Ref:** ISO 5167-2; Bernoulli with discharge coefficient.

## Pump — characteristic curve + affinity laws

Design data: `ratedFlow Qr`, `ratedHead Hr`, `shutoffHead H0`, `efficiency`,
`speedRatio s`, and an optional measured `(Q,H)` **head curve**.

Head characteristic `H(Q)` [m]:
```
if a (Q,H) curve with ≥3 points is supplied:
    least-squares quadratic fit  →  H(Q) = a0 + a1·Q + a2·Q²
else build from datasheet scalars:
    H(Q) = H0 + ((Hr − H0)/Qr²)·Q²        (a1 = 0)
```
Variable speed via the **affinity laws** (`Q ∝ s`, `H ∝ s²`); for the quadratic
this maps `a0 → s²a0`, `a1 → s·a1`, `a2 → a2`.

The pump raises outlet pressure, so `ΔP = −ρg·H(Q)`. For a given `ΔP` the operating
flow solves
```
a2·Q² + a1·Q + (a0 − Ht) = 0 ,     Ht = −ΔP/(ρg)
dQ/dΔP = −1 / [ (2a2·Q + a1)·ρg ]      (implicit differentiation)
```
The least-squares fit uses the in-tree solver (`src/core/CurveFit.cpp`,
normal equations via dense LU).
**Ref:** pump affinity laws; manufacturer characteristic curves.

## Heat Exchanger — shell-and-tube, tube side

Design data: `tubeLength L`, tube `ID`/`OD`, `numTubes N`, `numPasses`,
`roughness`, `minorK`, overall coefficient `U`.

Tube-side pressure drop (flow splits equally among `N` tubes,
`L_eff = L·passes`, `A_t = π·ID²/4`, `v_t = (Q/N)/A_t`):
```
ΔP = ( f·L_eff/ID + ΣK )·ρ v_t²/2     ⇒   K = ( f·L_eff/ID + ΣK )·ρ / (2 (N·A_t)²)
```
Thermal duty (documented; solved in a later phase):
```
Q̇ = U · A_s · ΔT_lm ,   A_s = N·π·OD·L     (LMTD method)
```
**Ref:** Kern, *Process Heat Transfer* (tube-side ΔP); LMTD method.

## Tank — vessel node + inventory

Design data: `diameter D`, `height`, `level`, top pressure `p_top`, `elevation`.
```
node pressure:   P = p_top + ρ g · level
inventory (transient):   d(level)/dt = Q_net / A ,   A = π D²/4
```
Integrated with explicit Euler between steady solves.
**Ref:** hydrostatics; mass balance.

## Boundary / Junction

- **Boundary:** fixed-pressure node, `P = pressure` — anchors the network.
- **Junction:** ideal node, zero pressure drop; merges all connected ports into one solver node.

---

## Network solution

Unknowns are the pressures at the free (non-pinned) nodes. Mass conservation at
each free node `i` gives `F_i(P) = Σ branch flows = 0`. The nonlinear system
`F(P) = 0` is solved by **Newton-Raphson**: `J·ΔP = −F` with a dense LU
factorization and a backtracking line search, where `J_ij = ∂F_i/∂P_j` is
assembled from each branch's `dQ/dΔP`. Transient runs integrate tank levels and
re-solve the steady system each timestep. See `src/solver/HydraulicSolver.cpp`.

## Control loops (Controller → valve)

A `Controller` is linked by tag (`measComp`, `measVar`, `output`) and evaluated
once per simulation cycle, before the hydraulic balance, using the previous
cycle's readings:
```
err = setpoint − measured                          (measured = level | flow | pressure)
u   = Kp·err + (Kp/Ti)·∫err dt                      (discrete PI, ∫ accumulated each cycle)
position = clamp(u, 0, 1)  →  driven onto the output valve/damper
```
Anti-windup freezes the integral while the output is saturated. Reverse-acting
loops use a negative `Kp`. The bundled `examples/control.umpnap` holds a tank
level at a 5 m setpoint by modulating its inlet valve. See
`src/solver/ControlSolver.cpp`.

Transmitters, switches, RTDs, gauges and actuators are tagged P&ID library
symbols; full signal-path wiring and discrete logic/state-machine execution are
a later phase.

## Pneumatic, filter and vessel models

The pneumatic elements reuse the hydraulic laws with Air as the working fluid: a
**Duct** is a Pipe, a **Damper** a Valve, and **Fan/Blower/Compressor** are pumps
(head curve / affinity). A **Filter/Strainer** is a fixed resistance from a rated
clean-element point, `K = ratedDP/ratedFlow²`. **PressurizedTank** and
**AirReceiver** are vessels with the same node + inventory model as Tank, the top
pressure being the blanket-gas pressure. Electrical components are configurable
library symbols pending a power-flow solver.

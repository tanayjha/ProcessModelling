# UMPNAP Component PRD / Specification Sheet

> **Generated** by `tools/gen_component_specs.cpp` from the live `ComponentRegistry` — do not hand-edit. Re-run `./build/gen_component_specs` after changing the registry or branch laws.

This is the per-component contract for every type in the UMPNAP library: its P&ID identity, ports (with media that gate connections), parameters (name / unit / default / bounds, stored in SI), governing law, and solver participation. Equation detail lives in [`EQUATIONS.md`](EQUATIONS.md); architecture in [`../CLAUDE.md`](../CLAUDE.md) and [`../README.md`](../README.md).

**Library total:** 48 component types.

## Contents

- [Hydraulic](#hydraulic) (12)
- [Air / Gas](#air--gas) (7)
- [Heat Transfer](#heat-transfer) (3)
- [Steam](#steam) (6)
- [Electrical](#electrical) (8)
- [Instrumentation & Control](#instrumentation--control) (12)

---

## Hydraulic

### Boundary

| Field | Value |
|---|---|
| **Type** | `Boundary` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `BND-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — fixed-pressure boundary node (anchors the network) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `p` | bidirectional | Process → Liquid |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `pressure` | Pa | 200000 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Fixed-pressure node: P = pressure (anchors network pressure).

### Tank

| Field | Value |
|---|---|
| **Type** | `Tank` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `TK-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — vessel node (pins node pressure from level + cover-gas; explicit-Euler inventory in transient) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `p` | bidirectional | Process → Liquid |
| `gas` | bidirectional | Gas |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `diameter` | m | 2 | 0.001 | — |
| `height` | m | 6 | 0 | — |
| `level` | m | 2 | 0 | — |
| `p_top` | Pa | 101300 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Vessel node + inventory: P = p_top + rho*g*level; d(level)/dt = Q_net/A, A=pi*D^2/4. The cover-gas tapping carries the vapour-space (Gas) network. Ref: hydrostatics; mass balance.

### Pipe

| Field | Value |
|---|---|
| **Type** | `Pipe` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `L-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (7):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `length` | m | 10 | 0 | — |
| `ID` | m | 0.1 | 0.001 | — |
| `OD` | m | 0.114 | 0.001 | — |
| `roughness` | m | 4.5e-05 | 0 | — |
| `minorK` | - | 0 | 0 | — |
| `dZ` | m | 0 | 0 | — |
| `tuning` | - | 1 | 0 | — |

**Governing law / behaviour:** Darcy-Weisbach + minor losses + static head:
P_in - P_out = (f*L/D + sum K)*rho*v^2/2*tuning + rho*g*dZ;  v=Q/A, A=pi*D^2/4 (D=ID); f = 64/Re (laminar) | Swamee-Jain (turbulent). Ref: Darcy-Weisbach; Colebrook-White.

### Valve

| Field | Value |
|---|---|
| **Type** | `Valve` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `FCV-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |
| `act` | bidirectional | Signal |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `Kv` | m3/h/bar^0.5 | 50 | 0.001 | — |
| `characteristic` | 0/1/2 | 1 | 0 | 2 |
| `rangeability` | - | 50 | 1.1 | — |
| `position` | - | 1 | 0 | 1 |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Control valve, Kv sizing (IEC 60534): Q[m3/h] = Kv*phi(x)*sqrt(dP[bar]/SG); phi: x (linear) | R^(x-1) (eq%) | sqrt(x) (quick-opening). Ref: IEC 60534-2-1.

### Orifice

| Field | Value |
|---|---|
| **Type** | `Orifice` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `FE-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `bore` | m | 0.05 | 0.001 | — |
| `pipeID` | m | 0.1 | 0.001 | — |
| `Cd` | - | 0.62 | 0.1 | 1 |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Thin-plate orifice (ISO 5167): Q = Cd/sqrt(1-beta^4)*A_o*sqrt(2*dP/rho); beta=bore/pipeID, A_o=pi*bore^2/4. Ref: ISO 5167-2.

### Pump

| Field | Value |
|---|---|
| **Type** | `Pump` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `P-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |
| `drive` | bidirectional | Electrical |

**Parameters** (6):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 0.05 | 0 | — |
| `ratedHead` | m | 50 | 0 | — |
| `shutoffHead` | m | 65 | 0 | — |
| `efficiency` | - | 0.75 | 0 | 1 |
| `speedRatio` | - | 1 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Rotodynamic characteristic: dP = -rho*g*H(Q), H(Q)=a0+a1*Q+a2*Q^2 (fitted from (Q,H) points, or built from shutoff/rated head). Affinity: H~s^2, Q~s. Ref: pump/fan affinity laws.

### Junction

| Field | Value |
|---|---|
| **Type** | `Junction` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `J-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — ideal node (zero-ΔP manifold merging connected ports) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `a` | bidirectional | Process → Liquid |
| `b` | bidirectional | Process → Liquid |
| `c` | bidirectional | Process → Liquid |

**Parameters** (0):

_none (ideal node)_

**Governing law / behaviour:** Ideal node: zero pressure drop; mass-conserving merge of all connected ports.

### Filter

| Field | Value |
|---|---|
| **Type** | `Filter` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `FL-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 0.05 | 1e-06 | — |
| `ratedDP` | Pa | 20000 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Fixed-resistance element from a rated dP at rated flow: K=ratedDP/ratedFlow^2; dP=K*Q*|Q|.

### Strainer

| Field | Value |
|---|---|
| **Type** | `Strainer` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `ST-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 0.05 | 1e-06 | — |
| `ratedDP` | Pa | 10000 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Fixed-resistance element from a rated dP at rated flow: K=ratedDP/ratedFlow^2; dP=K*Q*|Q|.

### ReliefValve

| Field | Value |
|---|---|
| **Type** | `ReliefValve` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `PSV-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `setpoint` | Pa | 800000 | 0 | — |
| `blowdown` | Pa | 50000 | 1 | — |
| `Kv` | m3/h/bar^0.5 | 40 | 0.001 | — |
| `position` | - | 0 | 0 | 1 |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Self-acting pressure relief / safety valve: phi = clamp((dP-setpoint)/blowdown, 0, 1); Kv_eff=Kv*phi; shut below setpoint; one-way (no reverse flow). Ref: API 520/526; IEC 60534-2-1.

### Header

| Field | Value |
|---|---|
| **Type** | `Header` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `HDR-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — ideal node (zero-ΔP manifold merging connected ports) |

**Ports** (4):

| Port | Role | Medium |
|---|---|---|
| `a` | bidirectional | Process → Liquid |
| `b` | bidirectional | Process → Liquid |
| `c` | bidirectional | Process → Liquid |
| `d` | bidirectional | Process → Liquid |

**Parameters** (0):

_none (ideal node)_

**Governing law / behaviour:** Ideal node: zero pressure drop; mass-conserving merge of all connected ports.

### PressurizedTank

| Field | Value |
|---|---|
| **Type** | `PressurizedTank` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `PTK-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — vessel node (pins node pressure from level + cover-gas; explicit-Euler inventory in transient) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `p` | bidirectional | Process → Liquid |
| `gas` | bidirectional | Gas |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `diameter` | m | 2 | 0.001 | — |
| `height` | m | 6 | 0 | — |
| `level` | m | 3 | 0 | — |
| `p_top` | Pa | 500000 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Vessel node + inventory: P = p_top + rho*g*level; d(level)/dt = Q_net/A, A=pi*D^2/4. The cover-gas tapping carries the vapour-space (Gas) network. Ref: hydrostatics; mass balance.

---

## Air / Gas

### Duct

| Field | Value |
|---|---|
| **Type** | `Duct` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `DCT-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |

**Parameters** (7):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `length` | m | 10 | 0 | — |
| `ID` | m | 0.3 | 0.001 | — |
| `OD` | m | 0.31 | 0.001 | — |
| `roughness` | m | 9e-05 | 0 | — |
| `minorK` | - | 0 | 0 | — |
| `dZ` | m | 0 | 0 | — |
| `tuning` | - | 1 | 0 | — |

**Governing law / behaviour:** Darcy-Weisbach + minor losses + static head:
P_in - P_out = (f*L/D + sum K)*rho*v^2/2*tuning + rho*g*dZ;  v=Q/A, A=pi*D^2/4 (D=ID); f = 64/Re (laminar) | Swamee-Jain (turbulent). Ref: Darcy-Weisbach; Colebrook-White.

### Damper

| Field | Value |
|---|---|
| **Type** | `Damper` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `DMP-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |
| `act` | bidirectional | Signal |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `Kv` | m3/h/bar^0.5 | 500 | 0.001 | — |
| `characteristic` | 0/1/2 | 0 | 0 | 2 |
| `rangeability` | - | 30 | 1.1 | — |
| `position` | - | 1 | 0 | 1 |

**Governing law / behaviour:** Control valve, Kv sizing (IEC 60534): Q[m3/h] = Kv*phi(x)*sqrt(dP[bar]/SG); phi: x (linear) | R^(x-1) (eq%) | sqrt(x) (quick-opening). Ref: IEC 60534-2-1.

### Fan

| Field | Value |
|---|---|
| **Type** | `Fan` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `FAN-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |
| `drive` | bidirectional | Electrical |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 2 | 0 | — |
| `ratedHead` | m | 30 | 0 | — |
| `shutoffHead` | m | 40 | 0 | — |
| `efficiency` | - | 0.7 | 0 | 1 |
| `speedRatio` | - | 1 | 0 | — |

**Governing law / behaviour:** Rotodynamic characteristic: dP = -rho*g*H(Q), H(Q)=a0+a1*Q+a2*Q^2 (fitted from (Q,H) points, or built from shutoff/rated head). Affinity: H~s^2, Q~s. Ref: pump/fan affinity laws.

### Blower

| Field | Value |
|---|---|
| **Type** | `Blower` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `BLW-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |
| `drive` | bidirectional | Electrical |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 1 | 0 | — |
| `ratedHead` | m | 80 | 0 | — |
| `shutoffHead` | m | 100 | 0 | — |
| `efficiency` | - | 0.7 | 0 | 1 |
| `speedRatio` | - | 1 | 0 | — |

**Governing law / behaviour:** Rotodynamic characteristic: dP = -rho*g*H(Q), H(Q)=a0+a1*Q+a2*Q^2 (fitted from (Q,H) points, or built from shutoff/rated head). Affinity: H~s^2, Q~s. Ref: pump/fan affinity laws.

### Compressor

| Field | Value |
|---|---|
| **Type** | `Compressor` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `CMP-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |
| `drive` | bidirectional | Electrical |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedFlow` | m3/s | 0.5 | 0 | — |
| `ratedHead` | m | 300 | 0 | — |
| `shutoffHead` | m | 400 | 0 | — |
| `efficiency` | - | 0.75 | 0 | 1 |
| `speedRatio` | - | 1 | 0 | — |

**Governing law / behaviour:** Rotodynamic characteristic: dP = -rho*g*H(Q), H(Q)=a0+a1*Q+a2*Q^2 (fitted from (Q,H) points, or built from shutoff/rated head). Affinity: H~s^2, Q~s. Ref: pump/fan affinity laws.

### AirReceiver

| Field | Value |
|---|---|
| **Type** | `AirReceiver` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `ARC-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — vessel node (pins node pressure from level + cover-gas; explicit-Euler inventory in transient) |

**Ports** (20):

| Port | Role | Medium |
|---|---|---|
| `p` | bidirectional | Process → Gas |
| `n1` | bidirectional | Process → Gas |
| `n2` | bidirectional | Process → Gas |
| `n3` | bidirectional | Process → Gas |
| `n4` | bidirectional | Process → Gas |
| `n5` | bidirectional | Process → Gas |
| `n6` | bidirectional | Process → Gas |
| `n7` | bidirectional | Process → Gas |
| `n8` | bidirectional | Process → Gas |
| `n9` | bidirectional | Process → Gas |
| `n10` | bidirectional | Process → Gas |
| `n11` | bidirectional | Process → Gas |
| `n12` | bidirectional | Process → Gas |
| `n13` | bidirectional | Process → Gas |
| `n14` | bidirectional | Process → Gas |
| `n15` | bidirectional | Process → Gas |
| `n16` | bidirectional | Process → Gas |
| `n17` | bidirectional | Process → Gas |
| `n18` | bidirectional | Process → Gas |
| `n19` | bidirectional | Process → Gas |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `diameter` | m | 1 | 0.001 | — |
| `height` | m | 2.5 | 0 | — |
| `level` | m | 0 | 0 | — |
| `p_top` | Pa | 700000 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Vessel node + inventory: P = p_top + rho*g*level; d(level)/dt = Q_net/A, A=pi*D^2/4. The cover-gas tapping carries the vapour-space (Gas) network. Ref: hydrostatics; mass balance.

### GasReliefValve

| Field | Value |
|---|---|
| **Type** | `GasReliefValve` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `PSV-` |
| **Default working fluid** | Air |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Gas |
| `out` | outlet | Process → Gas |

**Parameters** (5):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `setpoint` | Pa | 850000 | 0 | — |
| `blowdown` | Pa | 50000 | 1 | — |
| `Kv` | m3/h/bar^0.5 | 300 | 0.001 | — |
| `position` | - | 0 | 0 | 1 |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Self-acting pressure relief / safety valve: phi = clamp((dP-setpoint)/blowdown, 0, 1); Kv_eff=Kv*phi; shut below setpoint; one-way (no reverse flow). Ref: API 520/526; IEC 60534-2-1.

---

## Heat Transfer

### HeatExchanger

| Field | Value |
|---|---|
| **Type** | `HeatExchanger` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `HX-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (9):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `tubeLength` | m | 4 | 0 | — |
| `tubeID` | m | 0.016 | 0.001 | — |
| `tubeOD` | m | 0.019 | 0.001 | — |
| `numTubes` | - | 100 | 1 | — |
| `numPasses` | - | 2 | 1 | — |
| `roughness` | m | 1.5e-06 | 0 | — |
| `minorK` | - | 2 | 0 | — |
| `U` | W/m2K | 500 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Shell-and-tube, tube side: dP = (f*L_eff/d_i + K)*rho*v_t^2/2; v_t=(Q/N)/A_t, L_eff=L*passes; duty Qdot = U*A_s*LMTD. Ref: Kern; LMTD method.

### TubeSide

| Field | Value |
|---|---|
| **Type** | `TubeSide` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `HXT-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (10):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `tubeLength` | m | 4 | 0 | — |
| `tubeID` | m | 0.016 | 0.001 | — |
| `tubeOD` | m | 0.019 | 0.001 | — |
| `numTubes` | - | 100 | 1 | — |
| `numPasses` | - | 2 | 1 | — |
| `roughness` | m | 1.5e-06 | 0 | — |
| `minorK` | - | 2 | 0 | — |
| `U` | W/m2K | 500 | 0 | — |
| `Tin` | C | 90 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Split shell-and-tube, coupled by config["thermalTag"]: tube dP=(f*L_eff/d_i+K)*rho*v_t^2/2; shell dP=(crossLossK+K)*rho*v^2/2; duty by eps-NTU: NTU=UA/Cmin, C=rho*|Q|*cp, Qdot=eps*Cmin*(Th_in-Tc_in); UA=U*N*pi*d_o*L; outlets from energy balance. Ref: Kern; eps-NTU.

### ShellSide

| Field | Value |
|---|---|
| **Type** | `ShellSide` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `HXS-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — hydraulic/pneumatic branch (Newton-Raphson pressure-flow; signed Q) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Process → Liquid |
| `out` | outlet | Process → Liquid |

**Parameters** (8):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `shellID` | m | 0.4 | 0.001 | — |
| `baffleSpacing` | m | 0.3 | 0.001 | — |
| `tubeOD` | m | 0.019 | 0.001 | — |
| `pitchRatio` | - | 1.25 | 1 | — |
| `crossLossK` | - | 4 | 0 | — |
| `minorK` | - | 2 | 0 | — |
| `Tin` | C | 30 | 0 | — |
| `elevation` | m | 0 | 0 | — |

**Governing law / behaviour:** Split shell-and-tube, coupled by config["thermalTag"]: tube dP=(f*L_eff/d_i+K)*rho*v_t^2/2; shell dP=(crossLossK+K)*rho*v^2/2; duty by eps-NTU: NTU=UA/Cmin, C=rho*|Q|*cp, Qdot=eps*Cmin*(Th_in-Tc_in); UA=U*N*pi*d_o*L; outlets from energy balance. Ref: Kern; eps-NTU.

---

## Steam

### SteamGenerator

| Field | Value |
|---|---|
| **Type** | `SteamGenerator` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `SG-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — steam subnetwork (compressible isothermal pressure-flow; SteamSolver) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `feedwater` | inlet | Liquid |
| `steam` | outlet | Steam |
| `blowdown` | outlet | Liquid |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `pressure` | Pa | 4.7e+06 | 0 | — |
| `thermalPower` | MW | 600 | 0 | — |
| `tubeArea` | m2 | 3000 | 0 | — |
| `level` | m | 12 | 0 | — |

**Governing law / behaviour:** Steam source/sink node in the steam pressure-flow solve; raises feedwater (Liquid) to steam (Steam) at vessel pressure; blowdown Liquid let-down. Thermal power sets swallowing.

### Turbine

| Field | Value |
|---|---|
| **Type** | `Turbine` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `TUR-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — steam subnetwork (compressible isothermal pressure-flow; SteamSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `steam` | inlet | Steam |
| `exhaust` | outlet | Steam |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `ratedPower` | MW | 700 | 0 | — |
| `inletP` | Pa | 4.5e+06 | 0 | — |
| `exhaustP` | Pa | 5000 | 0 | — |
| `efficiency` | - | 0.85 | 0 | 1 |

**Governing law / behaviour:** Steam turbine (compressible swallowing): mdot = C*sign(s)*sqrt|s|, s = P_in^2 - P_exh^2; C from rated power / enthalpy drop; Power = mdot*dh*eta. Ref: Stodola ellipse.

### Condenser

| Field | Value |
|---|---|
| **Type** | `Condenser` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `CND-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — steam subnetwork (compressible isothermal pressure-flow; SteamSolver) |

**Ports** (4):

| Port | Role | Medium |
|---|---|---|
| `steam` | inlet | Steam |
| `condensate` | outlet | Liquid |
| `cwIn` | inlet | Process → Liquid |
| `cwOut` | outlet | Process → Liquid |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `pressure` | Pa | 5000 | 0 | — |
| `duty` | MW | 700 | 0 | — |
| `area` | m2 | 8000 | 0 | — |

**Governing law / behaviour:** Steam sink: condenses steam (Steam) to condensate (Liquid) at the condenser pressure; cooling-water Process side passes through. Duty doc'd; back-pressure pins the steam node.

### Deaerator

| Field | Value |
|---|---|
| **Type** | `Deaerator` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `DA-` |
| **Default working fluid** | Light Water |
| **Solver role** | Library model — steam/feedwater vessel (full two-phase thermo deferred) |

**Ports** (4):

| Port | Role | Medium |
|---|---|---|
| `feedwater` | inlet | Liquid |
| `steam` | inlet | Steam |
| `out` | outlet | Liquid |
| `gas` | bidirectional | Gas |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `pressure` | Pa | 120000 | 0 | — |
| `diameter` | m | 3 | 0.001 | — |
| `level` | m | 2 | 0 | — |
| `p_top` | Pa | 120000 | 0 | — |

**Governing law / behaviour:** Feedwater deaerating heater: mixes feedwater (Liquid) and heating steam (Steam), vents non-condensables via the gas tapping; vessel level/pressure tracked. Two-phase thermo deferred.

### ASDV

| Field | Value |
|---|---|
| **Type** | `ASDV` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `ASDV-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — steam subnetwork (compressible isothermal pressure-flow; SteamSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Steam |
| `out` | outlet | Steam |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `Kv` | m3/h/bar^0.5 | 1500 | 0.001 | — |
| `setpoint` | Pa | 5e+06 | 0 | — |
| `position` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Steam dump valve (atmospheric / condenser): compressible Kv flow mdot = Kv_eff*f(P_in,P_out), opens above setpoint. Relieves steam header pressure.

### CSDV

| Field | Value |
|---|---|
| **Type** | `CSDV` |
| **Domain** | Hydraulic |
| **P&ID tag prefix** | `CSDV-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — steam subnetwork (compressible isothermal pressure-flow; SteamSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | inlet | Steam |
| `out` | outlet | Steam |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `Kv` | m3/h/bar^0.5 | 2000 | 0.001 | — |
| `setpoint` | Pa | 4.8e+06 | 0 | — |
| `position` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Steam dump valve (atmospheric / condenser): compressible Kv flow mdot = Kv_eff*f(P_in,P_out), opens above setpoint. Relieves steam header pressure.

---

## Electrical

### Grid

| Field | Value |
|---|---|
| **Type** | `Grid` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `GRID-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `t` | bidirectional | Electrical |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `voltage` | V | 11000 | 0 | — |
| `frequency` | Hz | 50 | 0 | — |

**Governing law / behaviour:** Voltage source: pins terminal V in the DC power-flow. Ref: modified nodal analysis.

### Generator

| Field | Value |
|---|---|
| **Type** | `Generator` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `GEN-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `t` | bidirectional | Electrical |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rating` | kVA | 1000 | 0 | — |
| `voltage` | V | 415 | 0 | — |
| `pf` | - | 0.8 | 0 | 1 |

**Governing law / behaviour:** Voltage source: pins terminal V in the DC power-flow. Ref: modified nodal analysis.

### Transformer

| Field | Value |
|---|---|
| **Type** | `Transformer` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `TX-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `hv` | bidirectional | Electrical |
| `lv` | bidirectional | Electrical |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rating` | kVA | 1000 | 0 | — |
| `ratio` | - | 26.5 | 0 | — |
| `impedance` | % | 6 | 0 | — |

**Governing law / behaviour:** Ideal transformer (DC power-flow): V_hv = ratio*V_lv; power conserved; leakage impedance not modelled. Ref: MNA.

### Busbar

| Field | Value |
|---|---|
| **Type** | `Busbar` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `BUS-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (3):

| Port | Role | Medium |
|---|---|---|
| `a` | bidirectional | Electrical |
| `b` | bidirectional | Electrical |
| `c` | bidirectional | Electrical |

**Parameters** (1):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `voltage` | V | 415 | 0 | — |

**Governing law / behaviour:** Electrical node: common terminal joining connected electrical ports at one voltage.

### Breaker

| Field | Value |
|---|---|
| **Type** | `Breaker` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `CB-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | bidirectional | Electrical |
| `out` | bidirectional | Electrical |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rating` | A | 630 | 0 | — |
| `state` | 0/1 | 1 | 0 | 1 |

**Governing law / behaviour:** Switch: state=1 closes (near-zero R), state=0 opens (no current) in the power-flow.

### Cable

| Field | Value |
|---|---|
| **Type** | `Cable` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `W-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | bidirectional | Electrical |
| `out` | bidirectional | Electrical |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `length` | m | 50 | 0 | — |
| `area` | mm2 | 95 | 0 | — |

**Governing law / behaviour:** Series resistance: R = rho_cu*length/area; V=I*R in the nodal power-flow. Ref: resistive network.

### Motor

| Field | Value |
|---|---|
| **Type** | `Motor` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `M-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `t` | bidirectional | Electrical |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rating` | kW | 75 | 0 | — |
| `voltage` | V | 415 | 0 | — |
| `efficiency` | - | 0.92 | 0 | 1 |

**Governing law / behaviour:** Constant-power load as a conductance G = P/V^2 in the DC power-flow. Ref: MNA.

### ElectricalLoad

| Field | Value |
|---|---|
| **Type** | `ElectricalLoad` |
| **Domain** | Electrical |
| **P&ID tag prefix** | `LD-` |
| **Default working fluid** | Light Water |
| **Solver role** | Solving — electrical subnetwork (linear DC power-flow / MNA; ElectricalSolver) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `t` | bidirectional | Electrical |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `power` | kW | 50 | 0 | — |
| `pf` | - | 0.85 | 0 | 1 |

**Governing law / behaviour:** Constant-power load as a conductance G = P/V^2 in the DC power-flow. Ref: MNA.

---

## Instrumentation & Control

### Transmitter

| Field | Value |
|---|---|
| **Type** | `Transmitter` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `XT-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `measVar` | 0..3 | 0 | 0 | 3 |
| `rangeMin` | eu | 0 | 0 | — |
| `rangeMax` | eu | 100 | 0 | — |

**Governing law / behaviour:** Instrument element: reads measVar (0=P,1=Flow,2=Level,3=Temp) scaled to [rangeMin,rangeMax]; not solve-coupled (records derived signal). Wire 'sig' to a process component.

### Actuator

| Field | Value |
|---|---|
| **Type** | `Actuator` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `ACT-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `type` | 0/1 | 0 | 0 | 1 |
| `strokeTime` | s | 5 | 0 | — |
| `failPosition` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Valve actuator: wire 'sig' -> valve/damper 'act' to record the drive link; modulating throttles, on/off open/shut; failPosition on loss of motive power. Topological today (dynamic coupling deferred).

### ElectricActuator

| Field | Value |
|---|---|
| **Type** | `ElectricActuator` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `MOV-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `modulating` | 0/1 | 1 | 0 | 1 |
| `strokeTime` | s | 30 | 0 | — |
| `failPosition` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Valve actuator: wire 'sig' -> valve/damper 'act' to record the drive link; modulating throttles, on/off open/shut; failPosition on loss of motive power. Topological today (dynamic coupling deferred).

### ManualActuator

| Field | Value |
|---|---|
| **Type** | `ManualActuator` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `HW-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (1):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `turnsToOpen` | - | 12 | 0 | — |

**Governing law / behaviour:** Valve actuator: wire 'sig' -> valve/damper 'act' to record the drive link; modulating throttles, on/off open/shut; failPosition on loss of motive power. Topological today (dynamic coupling deferred).

### PneumaticActuatorModulating

| Field | Value |
|---|---|
| **Type** | `PneumaticActuatorModulating` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `PA-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `modulating` | 0/1 | 1 | 1 | 1 |
| `strokeTime` | s | 4 | 0 | — |
| `supplyP` | Pa | 400000 | 0 | — |
| `failPosition` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Valve actuator: wire 'sig' -> valve/damper 'act' to record the drive link; modulating throttles, on/off open/shut; failPosition on loss of motive power. Topological today (dynamic coupling deferred).

### PneumaticActuatorOnOff

| Field | Value |
|---|---|
| **Type** | `PneumaticActuatorOnOff` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `PAO-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `modulating` | 0/1 | 0 | 0 | — |
| `strokeTime` | s | 1.5 | 0 | — |
| `supplyP` | Pa | 400000 | 0 | — |
| `failPosition` | - | 0 | 0 | 1 |

**Governing law / behaviour:** Valve actuator: wire 'sig' -> valve/damper 'act' to record the drive link; modulating throttles, on/off open/shut; failPosition on loss of motive power. Topological today (dynamic coupling deferred).

### Switch

| Field | Value |
|---|---|
| **Type** | `Switch` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `XS-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (3):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `measVar` | 0..3 | 0 | 0 | 3 |
| `setpoint` | eu | 50 | 0 | — |
| `deadband` | eu | 1 | 0 | — |

**Governing law / behaviour:** Instrument element: reads measVar (0=P,1=Flow,2=Level,3=Temp) scaled to [rangeMin,rangeMax]; not solve-coupled (records derived signal). Wire 'sig' to a process component.

### RTD

| Field | Value |
|---|---|
| **Type** | `RTD` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `TE-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rangeMin` | C | 0 | 0 | — |
| `rangeMax` | C | 200 | 0 | — |

**Governing law / behaviour:** Instrument element: reads measVar (0=P,1=Flow,2=Level,3=Temp) scaled to [rangeMin,rangeMax]; not solve-coupled (records derived signal). Wire 'sig' to a process component.

### Gauge

| Field | Value |
|---|---|
| **Type** | `Gauge` |
| **Domain** | Instrument |
| **P&ID tag prefix** | `GI-` |
| **Default working fluid** | Light Water |
| **Solver role** | Non-solving — instrument symbol (tags + signal links; records derived signals only) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (2):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `rangeMin` | eu | 0 | 0 | — |
| `rangeMax` | eu | 16 | 0 | — |

**Governing law / behaviour:** Instrument element: reads measVar (0=P,1=Flow,2=Level,3=Temp) scaled to [rangeMin,rangeMax]; not solve-coupled (records derived signal). Wire 'sig' to a process component.

### Controller

| Field | Value |
|---|---|
| **Type** | `Controller` |
| **Domain** | Control |
| **P&ID tag prefix** | `IC-` |
| **Default working fluid** | Light Water |
| **Solver role** | Coupling — control block (ControlSolver / link; not a network node) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (4):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `setpoint` | eu | 50 | 0 | — |
| `gain` | - | 1 | 0 | — |
| `Ti` | s | 10 | 0 | — |
| `Td` | s | 0 | 0 | — |

**Governing law / behaviour:** PID controller: u = Kp*[e + (1/Ti)*integral(e) + Td*de/dt]; tag-linked to a measurement and an output valve (ControlSolver).

### Timer

| Field | Value |
|---|---|
| **Type** | `Timer` |
| **Domain** | Control |
| **P&ID tag prefix** | `TMR-` |
| **Default working fluid** | Light Water |
| **Solver role** | Coupling — control block (ControlSolver / link; not a network node) |

**Ports** (1):

| Port | Role | Medium |
|---|---|---|
| `sig` | bidirectional | Signal |

**Parameters** (1):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `preset` | s | 5 | 0 | — |

**Governing law / behaviour:** Discrete timer: preset delay; logic/sequencing block.

### Logic

| Field | Value |
|---|---|
| **Type** | `Logic` |
| **Domain** | Control |
| **P&ID tag prefix** | `LGC-` |
| **Default working fluid** | Light Water |
| **Solver role** | Coupling — control block (ControlSolver / link; not a network node) |

**Ports** (2):

| Port | Role | Medium |
|---|---|---|
| `in` | bidirectional | Signal |
| `out` | bidirectional | Signal |

**Parameters** (1):

| Parameter | Unit | Default | Min | Max |
|---|---|---|---|---|
| `function` | 0=AND/1=OR/2=NOT | 0 | 0 | 2 |

**Governing law / behaviour:** Discrete logic gate: function 0=AND, 1=OR, 2=NOT over signal inputs.


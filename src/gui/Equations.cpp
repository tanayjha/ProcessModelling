#include "gui/Equations.h"

namespace umpnap {

QString equationText(const std::string& type) {
  if (type == "Pipe")
    return "Darcy-Weisbach + minor losses + static head:\n"
           "  P_in−P_out = (f·L/D + ΣK)·ρv²/2·tuning + ρg·dZ\n"
           "  v = Q/A,  A = πD²/4  (D = ID)\n"
           "  dZ = outlet elev − inlet elev (+ = outlet higher)\n"
           "  f: 64/Re (laminar) | Swamee-Jain (turbulent)\n"
           "Ref: Darcy-Weisbach; Colebrook-White.";
  if (type == "Valve")
    return "Control valve (Kv sizing, IEC 60534):\n"
           "  Q[m³/h] = Kv_eff·√(ΔP[bar]/SG)\n"
           "  Kv_eff = Kv·φ(x)\n"
           "  φ: x (lin) | R^(x-1) (eq%) | √x (quick)\n"
           "Ref: IEC 60534-2-1.";
  if (type == "Orifice")
    return "Thin-plate orifice (ISO 5167):\n"
           "  Q = Cd/√(1-β⁴)·A_o·√(2ΔP/ρ)\n"
           "  β = bore/pipeID,  A_o = π·bore²/4\n"
           "Ref: ISO 5167-2.";
  if (type == "Pump")
    return "Centrifugal pump characteristic:\n"
           "  ΔP = -ρg·H(Q),  H(Q)=a0+a1Q+a2Q²\n"
           "  curve fitted from (Q,H) points, or built\n"
           "  from shutoff/rated head. Affinity: H~s², Q~s.\n"
           "Ref: pump affinity laws.";
  if (type == "HeatExchanger")
    return "Shell-and-tube, tube side:\n"
           "  ΔP = (f·L_eff/d_i + K)·ρv_t²/2\n"
           "  v_t = (Q/N)/A_t,  L_eff = L·passes\n"
           "  Duty (later): Q̇ = U·A_s·LMTD\n"
           "Ref: Kern; LMTD method.";
  if (type == "TubeSide" || type == "ShellSide")
    return "Split shell-and-tube exchanger (couple by thermalTag):\n"
           "  tube ΔP = (f·L_eff/d_i + K)·ρv_t²/2\n"
           "  shell ΔP = (crossLossK+K)·ρv²/2, v=Q/A_c\n"
           "  Duty by ε-NTU: NTU = UA/Cmin, C = ρ|Q|cp\n"
           "  ε(counterflow), Q̇ = ε·Cmin·(Th_in−Tc_in)\n"
           "  UA = U·N·π·d_o·L; outlets from energy balance.\n"
           "Ref: Kern; ε-NTU method.";
  if (type == "Turbine")
    return "Steam turbine (compressible swallowing):\n"
           "  ṁ = C·sign(s)·√|s|,  s = P_in²−P_exh²\n"
           "  C set from rated power / enthalpy drop\n"
           "  Power = ṁ·Δh·η.  Ref: Stodola ellipse.";
  if (type == "Transformer")
    return "Ideal transformer (DC power-flow):\n"
           "  V_hv = ratio·V_lv (turns ratio)\n"
           "  power conserved; leakage Z not modelled.\n"
           "Ref: modified nodal analysis.";
  if (type == "Cable" || type == "Grid" || type == "Generator" ||
      type == "Motor" || type == "ElectricalLoad" || type == "Busbar")
    return "Linear DC power-flow (V = I·R):\n"
           "  sources pin V; cables R=ρ_cu·L/A;\n"
           "  loads G=P/V²; solved by nodal analysis.\n"
           "Ref: resistive network / MNA.";
  if (type == "Tank")
    return "Vessel node + inventory:\n"
           "  P = p_top + ρg·level\n"
           "  d(level)/dt = Q_net/A,  A = πD²/4\n"
           "Ref: hydrostatics; mass balance.";
  if (type == "Boundary")
    return "Fixed-pressure node:\n  P = pressure (anchors the network).";
  if (type == "Junction")
    return "Ideal node: zero pressure drop; merges connected ports.";
  if (type == "Transmitter")
    return "Instrument (not solve-coupled in Phase 1):\n"
           "  reads measVar, scaled to [rangeMin, rangeMax].";
  if (type == "ReliefValve" || type == "GasReliefValve")
    return "Pressure relief / safety valve (self-acting):\n"
           "  φ = clamp((ΔP − setpoint)/blowdown, 0, 1)\n"
           "  Kv_eff = Kv·φ,  ΔP = Kv→K · Q·|Q|\n"
           "  shut below setpoint; one-way (no reverse flow).\n"
           "Ref: API 520/526; IEC 60534-2-1.";
  if (type == "Actuator" || type == "ElectricActuator" ||
      type == "ManualActuator" || type == "PneumaticActuatorModulating" ||
      type == "PneumaticActuatorOnOff")
    return "Valve actuator (drive link; not solve-coupled in Phase 1):\n"
           "  wire 'sig' → valve 'act' to record the driver.\n"
           "  modulating: throttles; on/off: open/shut;\n"
           "  fail position on loss of motive power.";
  if (type == "Controller")
    return "PID (not solve-coupled in Phase 1):\n"
           "  u = Kp·[e + (1/Ti)∫e dt + Td·de/dt].";
  return "";
}

}  // namespace umpnap

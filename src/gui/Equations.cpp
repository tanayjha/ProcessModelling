#include "gui/Equations.h"

namespace umpnap {

QString equationText(const std::string& type) {
  if (type == "Pipe")
    return "Darcy-Weisbach + minor losses:\n"
           "  ΔP = (f·L/D + ΣK)·ρv²/2·tuning\n"
           "  v = Q/A,  A = πD²/4  (D = ID)\n"
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
  if (type == "Actuator")
    return "Valve actuator (not solve-coupled in Phase 1):\n"
           "  drives valve position; stroke time, fail position.";
  if (type == "Controller")
    return "PID (not solve-coupled in Phase 1):\n"
           "  u = Kp·[e + (1/Ti)∫e dt + Td·de/dt].";
  return "";
}

}  // namespace umpnap

// Generates docs/COMPONENT_SPECS.md: a component-by-component PRD / spec sheet
// driven entirely off the ComponentRegistry, so it can never drift from the
// shipped library. Engine-only (no Qt). Usage:
//
//   ./build/gen_component_specs            # writes ../docs/COMPONENT_SPECS.md
//   ./build/gen_component_specs out.md     # writes the named file
//
// Re-run after touching ComponentRegistry.cpp / BranchLaw.cpp to refresh the doc.

#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "components/hydraulic/BranchLaw.h"
#include "core/Component.h"
#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"

using namespace umpnap;

namespace {

const char* roleName(PortRole r) {
  switch (r) {
    case PortRole::Inlet: return "inlet";
    case PortRole::Outlet: return "outlet";
    case PortRole::Bidirectional: return "bidirectional";
  }
  return "?";
}

const char* mediumName(Medium m) {
  switch (m) {
    case Medium::Process: return "Process";
    case Medium::Liquid: return "Liquid";
    case Medium::Gas: return "Gas";
    case Medium::Steam: return "Steam";
    case Medium::Electrical: return "Electrical";
    case Medium::Signal: return "Signal";
  }
  return "?";
}

// Medium shown for a port: a Process port resolves to its fluid's medium.
std::string portMedium(const Port& p, const std::string& fluid) {
  if (p.medium != Medium::Process) return mediumName(p.medium);
  Medium fm = fluidMedium(fluid);
  return std::string("Process \xE2\x86\x92 ") + mediumName(fm);  // arrow
}

// Library grouping mirrors gui/PaletteDock.cpp libraryOf().
std::string libraryOf(const ComponentDef& d) {
  static const std::set<std::string> air = {
      "Duct", "Damper", "Fan", "Blower", "Compressor", "AirReceiver",
      "GasReliefValve"};
  static const std::set<std::string> steam = {
      "SteamGenerator", "Turbine", "Condenser", "Deaerator", "ASDV", "CSDV"};
  static const std::set<std::string> heat = {"HeatExchanger", "ShellSide",
                                             "TubeSide"};
  if (d.domain == Domain::Electrical) return "Electrical";
  if (d.domain == Domain::Instrument || d.domain == Domain::Control)
    return "Instrumentation & Control";
  if (steam.count(d.type)) return "Steam";
  if (air.count(d.type)) return "Air / Gas";
  if (heat.count(d.type)) return "Heat Transfer";
  return "Hydraulic";
}

// One-line solver participation classification.
std::string solverRole(const ComponentDef& d) {
  static const std::set<std::string> steamSolved = {
      "SteamGenerator", "Turbine", "Condenser", "ASDV", "CSDV"};
  const std::string& t = d.type;
  if (isTankType(t))
    return "Solving \xE2\x80\x94 vessel node (pins node pressure from level + "
           "cover-gas; explicit-Euler inventory in transient)";
  if (t == "Boundary")
    return "Solving \xE2\x80\x94 fixed-pressure boundary node (anchors the "
           "network)";
  if (t == "Junction" || t == "Header")
    return "Solving \xE2\x80\x94 ideal node (zero-\xCE\x94P manifold merging "
           "connected ports)";
  if (isBranch(t))
    return "Solving \xE2\x80\x94 hydraulic/pneumatic branch (Newton-Raphson "
           "pressure-flow; signed Q)";
  if (steamSolved.count(t))
    return "Solving \xE2\x80\x94 steam subnetwork (compressible isothermal "
           "pressure-flow; SteamSolver)";
  if (t == "Deaerator")
    return "Library model \xE2\x80\x94 steam/feedwater vessel (full two-phase "
           "thermo deferred)";
  if (d.domain == Domain::Electrical)
    return "Solving \xE2\x80\x94 electrical subnetwork (linear DC power-flow / "
           "MNA; ElectricalSolver)";
  if (d.domain == Domain::Instrument)
    return "Non-solving \xE2\x80\x94 instrument symbol (tags + signal links; "
           "records derived signals only)";
  if (d.domain == Domain::Control)
    return "Coupling \xE2\x80\x94 control block (ControlSolver / link; not a "
           "network node)";
  return "Library symbol";
}

// Condensed ASCII governing law, mirroring gui/Equations.cpp equationText().
std::string equationSummary(const std::string& t) {
  if (t == "Pipe" || t == "Duct")
    return "Darcy-Weisbach + minor losses + static head:\n"
           "P_in - P_out = (f*L/D + sum K)*rho*v^2/2*tuning + rho*g*dZ;  "
           "v=Q/A, A=pi*D^2/4 (D=ID); f = 64/Re (laminar) | Swamee-Jain "
           "(turbulent). Ref: Darcy-Weisbach; Colebrook-White.";
  if (t == "Valve" || t == "Damper")
    return "Control valve, Kv sizing (IEC 60534): Q[m3/h] = "
           "Kv*phi(x)*sqrt(dP[bar]/SG); phi: x (linear) | R^(x-1) (eq%) | "
           "sqrt(x) (quick-opening). Ref: IEC 60534-2-1.";
  if (t == "Orifice")
    return "Thin-plate orifice (ISO 5167): Q = Cd/sqrt(1-beta^4)*A_o*"
           "sqrt(2*dP/rho); beta=bore/pipeID, A_o=pi*bore^2/4. "
           "Ref: ISO 5167-2.";
  if (t == "Pump" || t == "Fan" || t == "Blower" || t == "Compressor")
    return "Rotodynamic characteristic: dP = -rho*g*H(Q), H(Q)=a0+a1*Q+a2*Q^2 "
           "(fitted from (Q,H) points, or built from shutoff/rated head). "
           "Affinity: H~s^2, Q~s. Ref: pump/fan affinity laws.";
  if (t == "HeatExchanger")
    return "Shell-and-tube, tube side: dP = (f*L_eff/d_i + K)*rho*v_t^2/2; "
           "v_t=(Q/N)/A_t, L_eff=L*passes; duty Qdot = U*A_s*LMTD. "
           "Ref: Kern; LMTD method.";
  if (t == "TubeSide" || t == "ShellSide")
    return "Split shell-and-tube, coupled by config[\"thermalTag\"]: tube "
           "dP=(f*L_eff/d_i+K)*rho*v_t^2/2; shell dP=(crossLossK+K)*rho*v^2/2; "
           "duty by eps-NTU: NTU=UA/Cmin, C=rho*|Q|*cp, "
           "Qdot=eps*Cmin*(Th_in-Tc_in); UA=U*N*pi*d_o*L; outlets from "
           "energy balance. Ref: Kern; eps-NTU.";
  if (t == "Filter" || t == "Strainer")
    return "Fixed-resistance element from a rated dP at rated flow: "
           "K=ratedDP/ratedFlow^2; dP=K*Q*|Q|.";
  if (t == "ReliefValve" || t == "GasReliefValve")
    return "Self-acting pressure relief / safety valve: phi = "
           "clamp((dP-setpoint)/blowdown, 0, 1); Kv_eff=Kv*phi; shut below "
           "setpoint; one-way (no reverse flow). Ref: API 520/526; "
           "IEC 60534-2-1.";
  if (t == "Tank" || t == "PressurizedTank" || t == "AirReceiver")
    return "Vessel node + inventory: P = p_top + rho*g*level; "
           "d(level)/dt = Q_net/A, A=pi*D^2/4. The cover-gas tapping carries "
           "the vapour-space (Gas) network. Ref: hydrostatics; mass balance.";
  if (t == "Boundary")
    return "Fixed-pressure node: P = pressure (anchors network pressure).";
  if (t == "Junction" || t == "Header")
    return "Ideal node: zero pressure drop; mass-conserving merge of all "
           "connected ports.";
  if (t == "SteamGenerator")
    return "Steam source/sink node in the steam pressure-flow solve; raises "
           "feedwater (Liquid) to steam (Steam) at vessel pressure; blowdown "
           "Liquid let-down. Thermal power sets swallowing.";
  if (t == "Turbine")
    return "Steam turbine (compressible swallowing): mdot = C*sign(s)*sqrt|s|, "
           "s = P_in^2 - P_exh^2; C from rated power / enthalpy drop; "
           "Power = mdot*dh*eta. Ref: Stodola ellipse.";
  if (t == "Condenser")
    return "Steam sink: condenses steam (Steam) to condensate (Liquid) at the "
           "condenser pressure; cooling-water Process side passes through. "
           "Duty doc'd; back-pressure pins the steam node.";
  if (t == "Deaerator")
    return "Feedwater deaerating heater: mixes feedwater (Liquid) and heating "
           "steam (Steam), vents non-condensables via the gas tapping; vessel "
           "level/pressure tracked. Two-phase thermo deferred.";
  if (t == "ASDV" || t == "CSDV")
    return "Steam dump valve (atmospheric / condenser): compressible Kv flow "
           "mdot = Kv_eff*f(P_in,P_out), opens above setpoint. Relieves steam "
           "header pressure.";
  if (t == "Transformer")
    return "Ideal transformer (DC power-flow): V_hv = ratio*V_lv; power "
           "conserved; leakage impedance not modelled. Ref: MNA.";
  if (t == "Cable")
    return "Series resistance: R = rho_cu*length/area; V=I*R in the nodal "
           "power-flow. Ref: resistive network.";
  if (t == "Grid" || t == "Generator")
    return "Voltage source: pins terminal V in the DC power-flow. "
           "Ref: modified nodal analysis.";
  if (t == "Motor" || t == "ElectricalLoad")
    return "Constant-power load as a conductance G = P/V^2 in the DC "
           "power-flow. Ref: MNA.";
  if (t == "Busbar")
    return "Electrical node: common terminal joining connected electrical "
           "ports at one voltage.";
  if (t == "Breaker")
    return "Switch: state=1 closes (near-zero R), state=0 opens (no current) "
           "in the power-flow.";
  if (t == "Transmitter" || t == "Switch" || t == "RTD" || t == "Gauge")
    return "Instrument element: reads measVar (0=P,1=Flow,2=Level,3=Temp) "
           "scaled to [rangeMin,rangeMax]; not solve-coupled (records derived "
           "signal). Wire 'sig' to a process component.";
  if (t == "Actuator" || t == "ElectricActuator" || t == "ManualActuator" ||
      t == "PneumaticActuatorModulating" || t == "PneumaticActuatorOnOff")
    return "Valve actuator: wire 'sig' -> valve/damper 'act' to record the "
           "drive link; modulating throttles, on/off open/shut; failPosition "
           "on loss of motive power. Topological today (dynamic coupling "
           "deferred).";
  if (t == "Controller")
    return "PID controller: u = Kp*[e + (1/Ti)*integral(e) + Td*de/dt]; "
           "tag-linked to a measurement and an output valve (ControlSolver).";
  if (t == "Timer")
    return "Discrete timer: preset delay; logic/sequencing block.";
  if (t == "Logic")
    return "Discrete logic gate: function 0=AND, 1=OR, 2=NOT over signal "
           "inputs.";
  return "(see docs/EQUATIONS.md)";
}

}  // namespace

int main(int argc, char** argv) {
  registerHydraulicComponents();
  const auto& defs = ComponentRegistry::instance().defs();

  std::string outPath = argc > 1 ? argv[1] : "../docs/COMPONENT_SPECS.md";

  // Stable group order (matches the palette + reads top-down by domain).
  const std::vector<std::string> groupOrder = {
      "Hydraulic", "Air / Gas",   "Heat Transfer",
      "Steam",     "Electrical", "Instrumentation & Control"};

  std::ostringstream md;
  md << "# UMPNAP Component PRD / Specification Sheet\n\n";
  md << "> **Generated** by `tools/gen_component_specs.cpp` from the live "
        "`ComponentRegistry` \xE2\x80\x94 do not hand-edit. Re-run "
        "`./build/gen_component_specs` after changing the registry or branch "
        "laws.\n\n";
  md << "This is the per-component contract for every type in the UMPNAP "
        "library: its P&ID identity, ports (with media that gate "
        "connections), parameters (name / unit / default / bounds, stored in "
        "SI), governing law, and solver participation. Equation detail lives "
        "in [`EQUATIONS.md`](EQUATIONS.md); architecture in "
        "[`../CLAUDE.md`](../CLAUDE.md) and [`../README.md`](../README.md).\n\n";

  md << "**Library total:** " << defs.size() << " component types.\n\n";

  // Contents.
  md << "## Contents\n\n";
  for (const auto& g : groupOrder) {
    int n = 0;
    for (const auto& d : defs)
      if (libraryOf(d) == g) ++n;
    if (!n) continue;
    // GitHub heading slug: lowercase, drop punctuation, spaces -> '-'.
    md << "- [" << g << "](#";
    for (char ch : g) {
      if (ch == ' ')
        md << '-';
      else if (std::isalnum((unsigned char)ch) || ch == '-')
        md << (char)std::tolower(ch);
      // '/' and '&' are dropped
    }
    md << ") (" << n << ")\n";
  }
  md << "\n";

  for (const auto& g : groupOrder) {
    bool any = false;
    for (const auto& d : defs)
      if (libraryOf(d) == g) { any = true; break; }
    if (!any) continue;

    md << "---\n\n## " << g << "\n\n";

    for (const auto& d : defs) {
      if (libraryOf(d) != g) continue;

      md << "### " << d.type << "\n\n";
      md << "| Field | Value |\n|---|---|\n";
      md << "| **Type** | `" << d.type << "` |\n";
      md << "| **Domain** | " << domainName(d.domain) << " |\n";
      md << "| **P&ID tag prefix** | `" << d.tagPrefix << "-` |\n";
      md << "| **Default working fluid** | " << d.defaultFluid << " |\n";
      md << "| **Solver role** | " << solverRole(d) << " |\n\n";

      // Ports.
      md << "**Ports** (" << d.ports.size() << "):\n\n";
      if (d.ports.empty()) {
        md << "_none_\n\n";
      } else {
        md << "| Port | Role | Medium |\n|---|---|---|\n";
        for (const auto& p : d.ports)
          md << "| `" << p.name << "` | " << roleName(p.role) << " | "
             << portMedium(p, d.defaultFluid) << " |\n";
        md << "\n";
      }

      // Parameters.
      md << "**Parameters** (" << d.params.size() << "):\n\n";
      if (d.params.empty()) {
        md << "_none (ideal node)_\n\n";
      } else {
        md << "| Parameter | Unit | Default | Min | Max |\n"
              "|---|---|---|---|---|\n";
        for (const auto& ps : d.params) {
          md << "| `" << ps.name << "` | " << (ps.unit.empty() ? "-" : ps.unit)
             << " | " << ps.def << " | " << ps.min << " | ";
          if (ps.max == 0.0)
            md << "\xE2\x80\x94";  // em dash = unbounded above (registry: max==0)
          else
            md << ps.max;
          md << " |\n";
        }
        md << "\n";
      }

      // Governing law.
      md << "**Governing law / behaviour:** " << equationSummary(d.type)
         << "\n\n";
    }
  }

  std::ofstream f(outPath);
  if (!f) {
    std::cerr << "gen_component_specs: cannot write " << outPath << "\n";
    return 1;
  }
  f << md.str();
  std::cerr << "gen_component_specs: wrote " << outPath << " (" << defs.size()
            << " components)\n";
  return 0;
}

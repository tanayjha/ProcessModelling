#pragma once
#include "solver/ISolver.h"
#include "solver/NodeGraph.h"

namespace umpnap {

// Clusters the steam-medium ports into dense nodes (union over connections),
// matching the numbering used to key steamnode.<n>.* signals. Exposed so the
// trend display can label each steam bus by the tags of its equipment.
PortNodeMap buildSteamPortNodes(const Network& net);

// Pragmatic compressible steam network solver. Solves a nodal PRESSURE balance
// over the steam-medium subnetwork (ports whose effective medium is Steam),
// conserving MASS at each free node. Steam is treated as an ideal vapour at a
// fixed reference temperature (isothermal / known-density) — sensible pressures
// and flows, not two-phase quality tracking.
//
// Roles:
//   SteamGenerator.steam  pinned pressure source (its `pressure`)
//   Condenser.steam       pinned pressure sink   (its `pressure`)
//   Deaerator.steam       pinned pressure sink   (its `pressure`)
//   Turbine               flow branch in->exhaust (Stodola-style swallowing)
//   ASDV / CSDV           steam dump/relief valves (in->out)
//
// Each branch follows the isothermal compressible relation
//   mdot = C * sign(P1^2-P2^2) * sqrt(|P1^2 - P2^2|)   [kg/s]
// solved by Newton-Raphson with dense LU. Records steamnode.<n>.pressure,
// comp.<id>.flow (kg/s) and, for turbines, comp.<id>.power (W).
class SteamSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Thermal; }
  SolveReport solveSteady(Network& net, Results& out, double t) override;

  double tol = 1e-4;   // convergence on max |mass residual| (kg/s)
  int maxIters = 100;
};

}  // namespace umpnap

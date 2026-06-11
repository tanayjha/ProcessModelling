#pragma once
#include "solver/ISolver.h"
#include "solver/NodeGraph.h"

namespace umpnap {

// Clusters the electrical-medium ports into dense nodes (union over connections,
// busbars and closed breakers), matching the numbering used to key
// elecnode.<n>.* signals. Exposed so the trend display can label each bus by the
// tags of its connected equipment.
PortNodeMap buildElecPortNodes(const Network& net);

// Linear DC / resistive power-flow solver (modified nodal analysis). Genuinely
// solves V = I*R over the electrical-medium subnetwork:
//   Grid / Generator      ideal voltage source, terminal-to-ground at `voltage`
//   Busbar                ideal node (all terminals merged)
//   Cable                 series resistance R = rho_cu * L / A
//   Breaker               closed = ideal link (merged); open = removed
//   Transformer           ideal turns-ratio (V_hv = ratio * V_lv)
//   Motor / ElectricalLoad  conductance to ground G = P / V_rated^2
//
// Solves [G B; C 0][V; i] = [Iinj; Vs] by dense LU. Records elecnode.<n>.voltage
// and per-component comp.<id>.voltage / .current / .power. AC magnitudes, phase
// and turns-ratio leakage impedance are out of scope (a later dedicated phase).
class ElectricalSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Electrical; }
  SolveReport solveSteady(Network& net, Results& out, double t) override;
};

}  // namespace umpnap

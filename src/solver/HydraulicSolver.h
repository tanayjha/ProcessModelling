#pragma once
#include "solver/ISolver.h"

namespace umpnap {

// Single-phase incompressible hydraulic solver.
// Nodal pressure formulation solved by Newton-Raphson with dense LU.
class HydraulicSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Hydraulic; }
  SolveReport solveSteady(Network& net, Results& out, double t) override;

  double tol = 1e-6;     // convergence on max |mass residual| (m^3/s)
  int maxIters = 100;
};

}  // namespace umpnap

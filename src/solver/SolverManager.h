#pragma once
#include "solver/HydraulicSolver.h"
#include "solver/ISolver.h"

namespace umpnap {

// Coordinates the domain solvers. Phase 1 dispatches to the hydraulic solver and
// owns the transient timestep loop (integrates tank levels between steady solves).
class SolverManager {
 public:
  // Steady-state solve at t=0. Clears prior results.
  SolveReport runSteady(Network& net, Results& out);

  // Transient: integrate tank levels with explicit Euler, re-solving each step.
  // Returns the report from the final step.
  SolveReport runTransient(Network& net, Results& out, double dt, int steps);

  HydraulicSolver& hydraulic() { return hydraulic_; }

 private:
  HydraulicSolver hydraulic_;
};

}  // namespace umpnap

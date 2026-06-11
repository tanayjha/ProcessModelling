#pragma once
#include "solver/ElectricalSolver.h"
#include "solver/HydraulicSolver.h"
#include "solver/ISolver.h"
#include "solver/SteamSolver.h"

namespace umpnap {

// Coordinates the domain solvers. Each cycle runs, in order, the hydraulic
// nodal solve, the steam pressure-flow solve, the electrical DC power-flow
// solve, then the shell-and-tube thermal-coupling pass — all recording into the
// same Results. Owns the transient timestep loop (integrates tank levels
// between steady re-solves).
class SolverManager {
 public:
  // Steady-state solve at t=0. Clears prior results.
  SolveReport runSteady(Network& net, Results& out);

  // Transient: integrate tank levels with explicit Euler, re-solving each step.
  // Returns the report from the final step.
  SolveReport runTransient(Network& net, Results& out, double dt, int steps);

  // Advance exactly one transient cycle at time t: solve the steady balance then
  // integrate inventory (tank levels) by dt. Used by the interactive simulation
  // engine (Run / Single-Step) so one tick == one solver cycle.
  SolveReport stepTransient(Network& net, Results& out, double dt, double t);

  HydraulicSolver& hydraulic() { return hydraulic_; }

 private:
  // Runs the steam, electrical and thermal-coupling passes at time t.
  void solveAuxDomains(Network& net, Results& out, double t);

  HydraulicSolver hydraulic_;
  SteamSolver steam_;
  ElectricalSolver electrical_;
};

}  // namespace umpnap

#include "solver/SolverManager.h"

#include <algorithm>
#include <string>

#include "components/hydraulic/BranchLaw.h"
#include "core/Results.h"
#include "solver/ControlSolver.h"
#include "solver/NodeGraph.h"

namespace umpnap {

SolveReport SolverManager::runSteady(Network& net, Results& out) {
  out.clear();
  return hydraulic_.solveSteady(net, out, 0.0);
}

SolveReport SolverManager::stepTransient(Network& net, Results& out, double dt,
                                         double t) {
  // Control pass first: PID controllers act on the previous cycle's readings
  // and reposition their valves before this cycle's hydraulic balance.
  applyControls(net, out, dt);
  SolveReport rep = hydraulic_.solveSteady(net, out, t);

  // Integrate tank levels from net inflow at each tank node (explicit Euler).
  NodeGraph g = buildNodeGraph(net);
  for (auto& c : net.components()) {
    if (!isTankType(c->type)) continue;
    int tankNode = g.nodeOf(c->id, "p");
    double qnet = 0.0;  // m^3/s into the tank
    for (const auto& br : g.branches) {
      double q = out.latest("comp." + std::to_string(br.compId) + ".flow");
      if (br.outNode == tankNode) qnet += q;   // flow enters tank
      if (br.inNode == tankNode) qnet -= q;    // flow leaves tank
    }
    // Tank cross-sectional area from its diameter: A = pi*D^2/4.
    double d = c->param("diameter");
    double area = 3.14159265358979323846 * d * d / 4.0;
    if (area > 1e-9) {
      double newLevel = c->param("level") + dt * qnet / area;
      c->params["level"] = std::max(0.0, newLevel);
    }
  }
  return rep;
}

SolveReport SolverManager::runTransient(Network& net, Results& out, double dt,
                                        int steps) {
  out.clear();
  SolveReport rep;
  if (steps < 1) steps = 1;
  for (int s = 0; s < steps; ++s) rep = stepTransient(net, out, dt, s * dt);
  return rep;
}

}  // namespace umpnap

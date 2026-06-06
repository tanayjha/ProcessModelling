#include "solver/SolverManager.h"

#include <algorithm>
#include <string>

#include "core/Results.h"
#include "solver/NodeGraph.h"

namespace umpnap {

SolveReport SolverManager::runSteady(Network& net, Results& out) {
  out.clear();
  return hydraulic_.solveSteady(net, out, 0.0);
}

SolveReport SolverManager::runTransient(Network& net, Results& out, double dt,
                                        int steps) {
  out.clear();
  SolveReport rep;
  if (steps < 1) steps = 1;

  for (int s = 0; s < steps; ++s) {
    double t = s * dt;
    rep = hydraulic_.solveSteady(net, out, t);

    // Integrate tank levels from net inflow at each tank node.
    NodeGraph g = buildNodeGraph(net);
    for (auto& c : net.components()) {
      if (c->type != "Tank") continue;
      int tankNode = g.nodeOf(c->id, "p");
      double qnet = 0.0;  // m^3/s into the tank
      for (const auto& br : g.branches) {
        double q = out.latest("comp." + std::to_string(br.compId) + ".flow");
        if (br.outNode == tankNode) qnet += q;   // flow enters tank
        if (br.inNode == tankNode) qnet -= q;    // flow leaves tank
      }
      double area = c->param("area");
      if (area > 1e-9) {
        double newLevel = c->param("level") + dt * qnet / area;
        c->params["level"] = std::max(0.0, newLevel);
      }
    }
  }
  return rep;
}

}  // namespace umpnap

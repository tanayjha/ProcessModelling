#include "solver/ControlSolver.h"

#include <string>

#include "core/Network.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"

namespace umpnap {

namespace {
double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

// Current value of the measured process variable on component m.
double measuredValue(const Component* m, const std::string& var,
                     const Results& res, const NodeGraph& g) {
  if (var == "flow")
    return res.latest("comp." + std::to_string(m->id) + ".flow");
  if (var == "pressure") {
    std::string port = m->ports.empty() ? std::string() : m->ports.front().name;
    int n = g.nodeOf(m->id, port);
    if (n >= 0) return res.latest("node." + std::to_string(n) + ".pressure");
    return 0.0;
  }
  return m->param("level");  // default: tank level
}
}  // namespace

void applyControls(Network& net, const Results& res, double dt) {
  NodeGraph g = buildNodeGraph(net);  // for pressure measurements
  for (auto& c : net.components()) {
    if (c->type != "Controller") continue;
    Component* meas = net.componentByName(c->cfg("measComp"));
    Component* out = net.componentByName(c->cfg("output"));
    if (!meas || !out) continue;

    std::string var = c->cfg("measVar");
    double measured = measuredValue(meas, var, res, g);
    double sp = c->param("setpoint");
    double Kp = c->param("gain");
    double Ti = c->param("Ti");

    double err = sp - measured;
    double integ = c->param("_integral") + err * dt;
    double u = Kp * err + (Ti > 1e-6 ? (Kp / Ti) * integ : 0.0);
    double pos = clamp01(u);

    // Anti-windup: do not accumulate the integral while saturated.
    if ((pos <= 0.0 && u < 0.0) || (pos >= 1.0 && u > 0.0))
      integ = c->param("_integral");
    c->params["_integral"] = integ;
    c->params["output"] = pos;       // for runtime display
    out->params["position"] = pos;   // drive the linked valve/damper
  }
}

}  // namespace umpnap

// Generates examples/loop.umpnap (a closed heavy-water pump loop) using the
// engine, then loads it back and runs a steady solve as a self-check. The file
// format is identical to what the GUI's File > Save produces.
#include <cstdio>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include "core/Results.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main(int argc, char** argv) {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  Network net;

  // A closed loop anchored by a heavy-water tank and driven by a pump:
  //   Tank -> Pump -> Pipe -> HeatExchanger -> Valve -> Tank
  auto tank = reg.create("Tank");
  tank->fluid = "Heavy Water (D2O)";
  tank->params["diameter"] = 2.5;
  tank->params["level"] = 4.0;
  tank->x = 80;  tank->y = 240;
  int t = net.addComponent(std::move(tank));

  auto pump = reg.create("Pump");
  pump->fluid = "Heavy Water (D2O)";
  pump->params["shutoffHead"] = 70.0;
  pump->params["ratedHead"] = 55.0;
  pump->params["ratedFlow"] = 0.12;
  pump->x = 300; pump->y = 80;
  int p = net.addComponent(std::move(pump));

  auto pipe = reg.create("Pipe");
  pipe->fluid = "Heavy Water (D2O)";
  pipe->params["length"] = 25.0;
  pipe->params["ID"] = 0.2;
  pipe->x = 540; pipe->y = 80;
  int pi = net.addComponent(std::move(pipe));

  auto hx = reg.create("HeatExchanger");
  hx->fluid = "Heavy Water (D2O)";
  hx->params["numTubes"] = 200.0;
  hx->x = 540; hx->y = 240;
  int h = net.addComponent(std::move(hx));

  auto valve = reg.create("Valve");
  valve->fluid = "Heavy Water (D2O)";
  valve->params["Kv"] = 120.0;
  valve->params["position"] = 0.8;
  valve->x = 300; valve->y = 240;
  int v = net.addComponent(std::move(valve));

  net.connect(t, "p", p, "in");
  net.connect(p, "out", pi, "in");
  net.connect(pi, "out", h, "in");
  net.connect(h, "out", v, "in");
  net.connect(v, "out", t, "p");

  const char* path = (argc > 1) ? argv[1] : "examples/loop.umpnap";
  if (!saveProject(net, path)) {
    std::fprintf(stderr, "failed to save %s\n", path);
    return 1;
  }
  std::printf("Wrote %s\n", path);

  // Self-check: load it back and solve.
  Network loaded;
  if (!loadProject(loaded, path)) {
    std::fprintf(stderr, "failed to load %s\n", path);
    return 1;
  }
  std::string err = loaded.validate();
  if (!err.empty()) {
    std::fprintf(stderr, "validate failed: %s\n", err.c_str());
    return 1;
  }
  Results res;
  SolverManager mgr;
  SolveReport rep = mgr.runSteady(loaded, res);
  std::printf("Loaded %zu components, %zu connections.\n",
              loaded.components().size(), loaded.connections().size());
  std::printf("Steady solve: %s (iters=%d, residual=%.2e)\n", rep.message.c_str(),
              rep.iterations, rep.residual);
  std::printf("Loop flow through pump = %.5f m^3/s\n",
              res.latest("comp." + std::to_string(p) + ".flow"));
  std::printf("Pump head = %.3f m\n", res.latest("comp." + std::to_string(p) + ".head"));
  return rep.converged ? 0 : 2;
}

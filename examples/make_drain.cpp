// Generates examples/drain.umpnap: a tank draining through a valve to an
// atmospheric boundary. Unlike the closed loop, this tank's level genuinely
// changes over time, so a transient run shows a decaying level/flow curve.
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

  // Tank (5 m head) -> Valve -> atmospheric Boundary.
  auto tank = reg.create("Tank");
  tank->params["area"] = 5.0;
  tank->params["level"] = 5.0;
  tank->params["p_top"] = 1.013e5;
  tank->x = 80;  tank->y = 160;
  int t = net.addComponent(std::move(tank));

  auto valve = reg.create("Valve");
  valve->params["Kv"] = 150.0;
  valve->params["position"] = 1.0;
  valve->x = 320; valve->y = 160;
  int v = net.addComponent(std::move(valve));

  auto bnd = reg.create("Boundary");
  bnd->params["pressure"] = 1.013e5;  // atmospheric: tank drains until level ~0
  bnd->x = 560; bnd->y = 160;
  int b = net.addComponent(std::move(bnd));

  net.connect(t, "p", v, "in");
  net.connect(v, "out", b, "p");

  const char* path = (argc > 1) ? argv[1] : "examples/drain.umpnap";
  if (!saveProject(net, path)) {
    std::fprintf(stderr, "failed to save %s\n", path);
    return 1;
  }
  std::printf("Wrote %s\n", path);

  // Self-check: load and run a transient; print level vs time to prove movement.
  Network loaded;
  if (!loadProject(loaded, path)) return 1;
  Results res;
  SolverManager mgr;
  mgr.runTransient(loaded, res, /*dt=*/1.0, /*steps=*/30);
  const auto* level = res.series("comp." + std::to_string(t) + ".level");
  const auto* flow = res.series("comp." + std::to_string(v) + ".flow");
  std::printf("t      level(m)   flow(m^3/s)\n");
  if (level && flow)
    for (size_t i = 0; i < level->size(); i += 5)
      std::printf("%-5g  %-9.4f  %-.4f\n", (*level)[i].first, (*level)[i].second,
                  (*flow)[i].second);
  return 0;
}

// Builds a multi-domain plant project that exercises all of the newer solvers
// at once when run on the Integrated tab:
//   steam_island.umpnap   SteamGenerator -> Turbine -> Condenser  (steam solver)
//   power.umpnap          Grid -> Transformer -> Bus -> Cable -> Motor (DC solver)
//   moderator.umpnap      hot heavy-water stream through a TubeSide
//   service_water.umpnap  cold service-water stream through a ShellSide
// The TubeSide and ShellSide share config["thermalTag"]="HX1", so the merged
// integrated network couples them by the effectiveness-NTU thermal pass.
#include <cstdio>
#include <memory>
#include <string>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"

using namespace umpnap;

static std::unique_ptr<Component> at(std::unique_ptr<Component> c, double x, double y) {
  c->x = x; c->y = y; return c;
}

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  // ---- Steam island -------------------------------------------------------
  {
    Network n;
    int sg = n.addComponent(at(reg.create("SteamGenerator"), 80, 80));
    int tu = n.addComponent(at(reg.create("Turbine"), 320, 80));
    int cn = n.addComponent(at(reg.create("Condenser"), 560, 80));
    n.connect(sg, "steam", tu, "steam");
    n.connect(tu, "exhaust", cn, "steam");
    saveProject(n, "examples/steam_island.umpnap");
  }

  // ---- Electrical distribution -------------------------------------------
  {
    Network n;
    auto grid = reg.create("Grid"); grid->params["voltage"] = 11000.0;
    int g = n.addComponent(at(std::move(grid), 80, 80));
    int tx = n.addComponent(at(reg.create("Transformer"), 260, 80));
    int bus = n.addComponent(at(reg.create("Busbar"), 440, 120));
    int cab = n.addComponent(at(reg.create("Cable"), 620, 80));
    int mot = n.addComponent(at(reg.create("Motor"), 800, 80));
    auto ld = reg.create("ElectricalLoad"); ld->params["power"] = 120.0;
    int load = n.addComponent(at(std::move(ld), 620, 220));
    n.connect(g, "t", tx, "hv");
    n.connect(tx, "lv", bus, "a");
    n.connect(bus, "b", cab, "in");
    n.connect(cab, "out", mot, "t");
    n.connect(bus, "c", load, "t");
    saveProject(n, "examples/power.umpnap");
  }

  // ---- Moderator loop: hot heavy-water through the TUBE side -------------
  {
    Network n;
    auto bh = reg.create("Boundary"); bh->params["pressure"] = 4.0e5;
    bh->fluid = "Heavy Water (D2O)";
    int a = n.addComponent(at(std::move(bh), 80, 80));
    auto tube = reg.create("TubeSide");
    tube->fluid = "Heavy Water (D2O)";
    tube->params["Tin"] = 95.0;            // hot moderator return
    tube->config["thermalTag"] = "HX1";
    int t = n.addComponent(at(std::move(tube), 300, 80));
    auto bl = reg.create("Boundary"); bl->params["pressure"] = 1.0e5;
    bl->fluid = "Heavy Water (D2O)";
    int b = n.addComponent(at(std::move(bl), 520, 80));
    n.connect(a, "p", t, "in");
    n.connect(t, "out", b, "p");
    saveProject(n, "examples/moderator.umpnap");
  }

  // ---- Service-water loop: cold water through the SHELL side -------------
  {
    Network n;
    auto bh = reg.create("Boundary"); bh->params["pressure"] = 3.5e5;
    int a = n.addComponent(at(std::move(bh), 80, 80));
    auto shell = reg.create("ShellSide");
    shell->params["Tin"] = 28.0;           // cold service water
    shell->config["thermalTag"] = "HX1";
    int s = n.addComponent(at(std::move(shell), 300, 80));
    auto bl = reg.create("Boundary"); bl->params["pressure"] = 1.0e5;
    int b = n.addComponent(at(std::move(bl), 520, 80));
    n.connect(a, "p", s, "in");
    n.connect(s, "out", b, "p");
    saveProject(n, "examples/service_water.umpnap");
  }

  PlantProject proj;
  proj.name = "Multi-Domain Demo";
  proj.mimics = {"steam_island.umpnap", "power.umpnap", "moderator.umpnap",
                 "service_water.umpnap"};
  savePlant(proj, "examples/multidomain.umpproj");
  std::printf("Wrote examples/multidomain.umpproj + 4 mimics\n");
  return 0;
}

// Closed-loop tank-level control demo. A PI controller measures tank level and
// modulates the inlet valve to hold a setpoint, while the tank drains through a
// fixed orifice. Proves instrumentation->controller->valve coupling in the
// transient engine. Generates examples/control.umpnap and runs the loop.
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

  auto supply = reg.create("Boundary");
  supply->name = "BND-SUP";
  supply->params["pressure"] = 4.0e5;
  supply->x = 60; supply->y = 80;
  int isup = net.addComponent(std::move(supply));

  auto valve = reg.create("Valve");
  valve->name = "FCV-1";
  valve->params["Kv"] = 80.0;
  valve->params["position"] = 0.3;
  valve->x = 260; valve->y = 80;
  int iv = net.addComponent(std::move(valve));

  auto tank = reg.create("Tank");
  tank->name = "TK-1";
  tank->params["diameter"] = 1.5;
  tank->params["level"] = 3.5;
  tank->x = 470; tank->y = 150;
  int it = net.addComponent(std::move(tank));

  auto orifice = reg.create("Orifice");
  orifice->name = "FE-1";
  orifice->params["bore"] = 0.03;
  orifice->x = 470; orifice->y = 330;
  int io = net.addComponent(std::move(orifice));

  auto drain = reg.create("Boundary");
  drain->name = "BND-DRN";
  drain->params["pressure"] = 1.013e5;
  drain->x = 60; drain->y = 330;
  int idr = net.addComponent(std::move(drain));

  auto ctrl = reg.create("Controller");
  ctrl->name = "LIC-1";
  ctrl->params["setpoint"] = 5.0;  // hold 5 m
  ctrl->params["gain"] = 0.4;
  ctrl->params["Ti"] = 30.0;
  ctrl->config["measComp"] = "TK-1";
  ctrl->config["measVar"] = "level";
  ctrl->config["output"] = "FCV-1";
  ctrl->x = 260; ctrl->y = 300;
  net.addComponent(std::move(ctrl));

  net.connect(isup, "p", iv, "in");
  net.connect(iv, "out", it, "p");
  net.connect(it, "p", io, "in");
  net.connect(io, "out", idr, "p");

  const char* path = (argc > 1) ? argv[1] : "examples/control.umpnap";
  saveProject(net, path);
  std::printf("Wrote %s\n", path);

  Network loaded;
  loadProject(loaded, path);
  Results res;
  SolverManager mgr;
  mgr.runSteady(loaded, res);
  std::printf("PI level control, setpoint = 5.0 m\n t     level   valve%%\n");
  for (int s = 1; s <= 300; ++s) {
    mgr.stepTransient(loaded, res, 1.0, s);
    if (s % 25 == 0) {
      Component* tk = loaded.componentByName("TK-1");
      Component* fv = loaded.componentByName("FCV-1");
      std::printf("%-5d  %.3f   %.0f%%\n", s, tk->param("level"),
                  fv->param("position") * 100.0);
    }
  }
  return 0;
}

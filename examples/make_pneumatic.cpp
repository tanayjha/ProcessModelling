// Pneumatic (air) example proving the new library types solve on the existing
// nodal engine: a blower drives air through a duct, damper and filter between
// two atmospheric boundaries. Generates examples/pneumatic.umpnap and solves it.
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

  auto a = reg.create("Boundary");
  a->fluid = "Air";
  a->params["pressure"] = 1.013e5;
  a->x = 60; a->y = 200;
  int ia = net.addComponent(std::move(a));

  auto blower = reg.create("Blower");  // defaults to Air
  blower->x = 240; blower->y = 80;
  int ib = net.addComponent(std::move(blower));

  auto duct = reg.create("Duct");
  duct->x = 430; duct->y = 80;
  int id = net.addComponent(std::move(duct));

  auto damper = reg.create("Damper");
  damper->params["position"] = 0.7;
  damper->x = 620; damper->y = 80;
  int idm = net.addComponent(std::move(damper));

  auto filter = reg.create("Filter");
  filter->fluid = "Air";
  filter->x = 430; filter->y = 320;
  int ifl = net.addComponent(std::move(filter));

  auto b = reg.create("Boundary");
  b->fluid = "Air";
  b->params["pressure"] = 1.013e5;
  b->x = 60; b->y = 320;
  int ibd = net.addComponent(std::move(b));

  net.connect(ia, "p", ib, "in");
  net.connect(ib, "out", id, "in");
  net.connect(id, "out", idm, "in");
  net.connect(idm, "out", ifl, "in");
  net.connect(ifl, "out", ibd, "p");

  const char* path = (argc > 1) ? argv[1] : "examples/pneumatic.umpnap";
  saveProject(net, path);
  std::printf("Wrote %s\n", path);

  Network loaded;
  loadProject(loaded, path);
  std::string err = loaded.validate();
  if (!err.empty()) { std::fprintf(stderr, "validate: %s\n", err.c_str()); return 1; }
  Results res;
  SolverManager mgr;
  SolveReport rep = mgr.runSteady(loaded, res);
  std::printf("Steady: %s (iters=%d)\n", rep.message.c_str(), rep.iterations);
  std::printf("Air flow through blower = %.4f m^3/s\n",
              res.latest("comp." + std::to_string(ib) + ".flow"));
  return rep.converged ? 0 : 2;
}

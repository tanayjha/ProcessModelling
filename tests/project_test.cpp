#include <cstdio>

#include "check.h"
#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  Network net;
  auto bA = reg.create("Boundary");
  bA->params["pressure"] = 2.5e5;
  bA->x = 10;
  bA->y = 20;
  int a = net.addComponent(std::move(bA));
  auto pipe = reg.create("Pipe");
  pipe->params["length"] = 42.0;
  pipe->fluid = "Heavy Water (D2O)";
  int p = net.addComponent(std::move(pipe));
  int b = net.addComponent(reg.create("Boundary"));
  net.connect(a, "p", p, "in");
  net.connect(p, "out", b, "p");

  const char* path = "umpnap_roundtrip_test.umpnap";
  CHECK(saveProject(net, path));

  Network loaded;
  CHECK(loadProject(loaded, path));
  CHECK(loaded.components().size() == 3);
  CHECK(loaded.connections().size() == 2);

  const Component* lp = loaded.component(p);
  CHECK(lp != nullptr);
  CHECK_NEAR(lp->param("length"), 42.0, 1e-9);
  CHECK(lp->fluid == "Heavy Water (D2O)");

  const Component* la = loaded.component(a);
  CHECK(la != nullptr);
  CHECK_NEAR(la->param("pressure"), 2.5e5, 1e-3);
  CHECK_NEAR(la->x, 10.0, 1e-9);

  std::remove(path);
  return REPORT();
}

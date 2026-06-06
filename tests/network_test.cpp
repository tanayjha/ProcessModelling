#include "check.h"
#include "core/ComponentRegistry.h"
#include "core/Network.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  Network net;
  int b = net.addComponent(reg.create("Boundary"));
  int p = net.addComponent(reg.create("Pipe"));
  int b2 = net.addComponent(reg.create("Boundary"));
  CHECK(net.components().size() == 3);

  net.connect(b, "p", p, "in");
  CHECK(net.connections().size() == 1);

  // Pipe 'out' dangling -> invalid.
  CHECK(!net.validate().empty());

  net.connect(p, "out", b2, "p");
  CHECK(net.validate().empty());

  // Remove pipe drops its connections.
  net.removeComponent(p);
  CHECK(net.components().size() == 2);
  CHECK(net.connections().empty());

  // Empty / no-anchor checks.
  Network n2;
  CHECK(!n2.validate().empty());  // empty
  n2.addComponent(reg.create("Pipe"));
  CHECK(!n2.validate().empty());  // no boundary/tank anchor

  return REPORT();
}

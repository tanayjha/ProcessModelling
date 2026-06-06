#include "check.h"
#include "core/ComponentRegistry.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  registerHydraulicComponents();  // idempotent
  auto& reg = ComponentRegistry::instance();

  CHECK(reg.defs().size() >= 8);
  CHECK(reg.find("Pipe") != nullptr);
  CHECK(reg.find("DoesNotExist") == nullptr);

  auto pipe = reg.create("Pipe");
  CHECK(pipe != nullptr);
  CHECK(pipe->hasPort("in"));
  CHECK(pipe->hasPort("out"));
  CHECK_NEAR(pipe->param("length"), 10.0, 1e-9);
  CHECK_NEAR(pipe->param("diameter"), 0.1, 1e-9);
  CHECK(pipe->fluid == "Light Water");

  CHECK(reg.create("Nope") == nullptr);
  return REPORT();
}

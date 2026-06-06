#include "check.h"
#include "core/FluidLibrary.h"

using namespace umpnap;

int main() {
  CHECK(FluidLibrary::names().size() == 6);
  CHECK(FluidLibrary::has("Light Water"));
  CHECK(FluidLibrary::has("Heavy Water (D2O)"));
  CHECK(!FluidLibrary::has("Plasma"));
  CHECK_NEAR(FluidLibrary::props("Heavy Water (D2O)").density, 1105.0, 1e-6);
  CHECK_NEAR(FluidLibrary::props("Light Water").density, 998.0, 1e-6);
  // Unknown falls back to Light Water.
  CHECK_NEAR(FluidLibrary::props("Plasma").density, 998.0, 1e-6);
  return REPORT();
}

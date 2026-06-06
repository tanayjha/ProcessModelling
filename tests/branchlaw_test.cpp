#include <cmath>

#include "check.h"
#include "components/hydraulic/BranchLaw.h"
#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  FluidProps water = FluidLibrary::props("Light Water");

  auto orifice = reg.create("Orifice");

  // Positive and negative dP give symmetric, oppositely-signed flow.
  BranchEval ep = evalBranch(*orifice, 1e4, water);
  BranchEval en = evalBranch(*orifice, -1e4, water);
  CHECK(ep.Q > 0);
  CHECK(en.Q < 0);
  CHECK_NEAR(ep.Q, -en.Q, 1e-9);

  // Consistency: K*Q*|Q| ~ dP. Recover K = dP / (Q*|Q|).
  double K = 1e4 / (ep.Q * std::fabs(ep.Q));
  CHECK(K > 0);
  CHECK(ep.dQ_ddP > 0);

  // Near zero dP the derivative stays finite (linearised region).
  BranchEval e0 = evalBranch(*orifice, 0.0, water);
  CHECK(std::isfinite(e0.dQ_ddP));
  CHECK_NEAR(e0.Q, 0.0, 1e-6);

  // Pump produces forward flow when head is available (dP near -rho*g*H0..0).
  auto pump = reg.create("Pump");
  BranchEval pe = evalBranch(*pump, -1.0e5, water);  // outlet higher than inlet
  CHECK(pe.Q > 0);

  // Friction factor: laminar branch.
  CHECK_NEAR(frictionFactor(1000.0, 0.0), 64.0 / 1000.0, 1e-12);
  // Turbulent branch is positive and smaller than laminar at same-ish Re.
  CHECK(frictionFactor(1e5, 1e-3) > 0);

  // isBranch classification.
  CHECK(isBranch("Pipe"));
  CHECK(!isBranch("Boundary"));
  CHECK(!isBranch("Junction"));

  return REPORT();
}

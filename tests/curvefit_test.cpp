#include "check.h"
#include "core/CurveFit.h"

using namespace umpnap;

int main() {
  // Exact quadratic y = 2 + 3x - 0.5 x^2 sampled at 5 points -> recover coeffs.
  std::vector<std::pair<double, double>> pts;
  for (double x : {0.0, 1.0, 2.0, 3.0, 4.0})
    pts.push_back({x, 2.0 + 3.0 * x - 0.5 * x * x});
  auto c = polyFit(pts, 2);
  CHECK(c.size() == 3);
  CHECK_NEAR(c[0], 2.0, 1e-6);
  CHECK_NEAR(c[1], 3.0, 1e-6);
  CHECK_NEAR(c[2], -0.5, 1e-6);

  // polyEval matches.
  CHECK_NEAR(polyEval(c, 2.0), 2.0 + 6.0 - 2.0, 1e-6);

  // Too few points for the degree -> empty.
  std::vector<std::pair<double, double>> few = {{0, 1}, {1, 2}};
  CHECK(polyFit(few, 2).empty());

  return REPORT();
}

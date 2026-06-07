#include "core/CurveFit.h"

#include <cmath>

#include "solver/LinAlg.h"

namespace umpnap {

std::vector<double> polyFit(const std::vector<std::pair<double, double>>& pts,
                            int degree) {
  const int m = degree + 1;
  if ((int)pts.size() < m) return {};

  // Build the normal-equations system  A c = b, where
  //   A[i][j] = sum_k x_k^(i+j),   b[i] = sum_k y_k * x_k^i.
  // This is X^T X c = X^T y for the Vandermonde design matrix X.
  Matrix A(m);
  std::vector<double> b(m, 0.0);
  for (const auto& p : pts) {
    double x = p.first, y = p.second;
    // Precompute powers x^0..x^(2*degree).
    std::vector<double> pw(2 * degree + 1, 1.0);
    for (int k = 1; k <= 2 * degree; ++k) pw[k] = pw[k - 1] * x;
    for (int i = 0; i < m; ++i) {
      for (int j = 0; j < m; ++j) A.at(i, j) += pw[i + j];
      b[i] += y * pw[i];
    }
  }
  if (!lu_solve(A, b)) return {};
  return b;  // overwritten with the solution coefficients
}

double polyEval(const std::vector<double>& c, double x) {
  // Horner's rule.
  double r = 0.0;
  for (int i = (int)c.size() - 1; i >= 0; --i) r = r * x + c[i];
  return r;
}

}  // namespace umpnap

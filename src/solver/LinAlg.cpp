#include "solver/LinAlg.h"
#include <cmath>

namespace umpnap {

bool lu_solve(Matrix A, std::vector<double>& b) {
  const int n = A.n;
  if (n == 0 || (int)b.size() != n) return false;

  // Gaussian elimination with partial pivoting, applied to A and b together.
  for (int col = 0; col < n; ++col) {
    // Find pivot row (largest magnitude in this column at/below the diagonal).
    int piv = col;
    double best = std::fabs(A.at(col, col));
    for (int r = col + 1; r < n; ++r) {
      double v = std::fabs(A.at(r, col));
      if (v > best) { best = v; piv = r; }
    }
    if (best < 1e-14) return false;  // singular

    if (piv != col) {
      for (int c = 0; c < n; ++c) std::swap(A.at(col, c), A.at(piv, c));
      std::swap(b[col], b[piv]);
    }

    // Eliminate below.
    double diag = A.at(col, col);
    for (int r = col + 1; r < n; ++r) {
      double factor = A.at(r, col) / diag;
      if (factor == 0.0) continue;
      for (int c = col; c < n; ++c) A.at(r, c) -= factor * A.at(col, c);
      b[r] -= factor * b[col];
    }
  }

  // Back-substitution.
  for (int r = n - 1; r >= 0; --r) {
    double s = b[r];
    for (int c = r + 1; c < n; ++c) s -= A.at(r, c) * b[c];
    b[r] = s / A.at(r, r);
  }
  return true;
}

}  // namespace umpnap

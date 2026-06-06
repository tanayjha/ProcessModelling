#pragma once
#include <vector>

namespace umpnap {

// Row-major dense square matrix.
struct Matrix {
  int n = 0;
  std::vector<double> a;  // n*n, row-major
  Matrix() = default;
  explicit Matrix(int n_) : n(n_), a((size_t)n_ * n_, 0.0) {}
  double& at(int r, int c) { return a[(size_t)r * n + c]; }
  double at(int r, int c) const { return a[(size_t)r * n + c]; }
};

// Solves A x = b via LU with partial pivoting. A is taken by value (copied).
// b is overwritten with the solution x. Returns false if A is singular.
bool lu_solve(Matrix A, std::vector<double>& b);

}  // namespace umpnap

#include "check.h"
#include "solver/LinAlg.h"

using namespace umpnap;

int main() {
  // 2x2: [[2,1],[1,3]] x = [3,5] -> x = [0.8, 1.4]
  {
    Matrix A(2);
    A.at(0, 0) = 2; A.at(0, 1) = 1;
    A.at(1, 0) = 1; A.at(1, 1) = 3;
    std::vector<double> b = {3, 5};
    CHECK(lu_solve(A, b));
    CHECK_NEAR(b[0], 0.8, 1e-9);
    CHECK_NEAR(b[1], 1.4, 1e-9);
  }
  // 3x3 known system. A x = b with x = [1,2,3].
  {
    Matrix A(3);
    double vals[9] = {2, 1, 1, 1, 3, 2, 1, 0, 4};
    for (int i = 0; i < 9; ++i) A.a[i] = vals[i];
    // b = A * [1,2,3]
    std::vector<double> b = {2 + 2 + 3, 1 + 6 + 6, 1 + 0 + 12};
    CHECK(lu_solve(A, b));
    CHECK_NEAR(b[0], 1.0, 1e-9);
    CHECK_NEAR(b[1], 2.0, 1e-9);
    CHECK_NEAR(b[2], 3.0, 1e-9);
  }
  // Singular matrix -> false.
  {
    Matrix A(2);
    A.at(0, 0) = 1; A.at(0, 1) = 2;
    A.at(1, 0) = 2; A.at(1, 1) = 4;
    std::vector<double> b = {1, 2};
    CHECK(!lu_solve(A, b));
  }
  return REPORT();
}

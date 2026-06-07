#pragma once
#include <utility>
#include <vector>

namespace umpnap {

// Least-squares polynomial fit y(x) = c0 + c1 x + c2 x^2 + ... + c_deg x^deg.
// Returns coefficients [c0..c_deg] (size deg+1). On failure (too few points or
// singular system) returns an empty vector.
//
// Method: normal equations  (X^T X) c = X^T y, solved with the in-tree dense LU.
// For a pump head curve the caller uses degree 2, giving H(Q)=c0+c1 Q+c2 Q^2.
std::vector<double> polyFit(const std::vector<std::pair<double, double>>& pts,
                            int degree);

// Evaluate a fitted polynomial at x.
double polyEval(const std::vector<double>& coeffs, double x);

}  // namespace umpnap

#pragma once
#include <string>

namespace umpnap {

struct ValidationResult {
  bool passed = false;
  double maxRelError = 0.0;
  std::string detail;
};

// Builds a series+parallel orifice network with a closed-form analytical answer,
// solves it, and compares. Orifices are used so resistance is flow-independent
// and the analytical solution is exact.
ValidationResult runPipeNetworkValidation(double tol = 1e-3);

}  // namespace umpnap

#pragma once
#include <string>

#include "core/Component.h"
#include "core/Network.h"

namespace umpnap {

class Results;

struct SolveReport {
  bool converged = false;
  int iterations = 0;
  double residual = 0.0;
  std::string message;
};

// A domain solver. Hydraulic is implemented; gas/thermal/electrical are stubs.
class ISolver {
 public:
  virtual ~ISolver() = default;
  virtual Domain domain() const = 0;
  // Solve the steady state at time t, recording results into out.
  virtual SolveReport solveSteady(Network& net, Results& out, double t) = 0;
};

}  // namespace umpnap

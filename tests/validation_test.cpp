#include <cstdio>

#include "check.h"
#include "solver/Validation.h"

using namespace umpnap;

int main() {
  ValidationResult vr = runPipeNetworkValidation(1e-3);
  std::printf("%s\n", vr.detail.c_str());
  std::printf("maxRelError = %.3e\n", vr.maxRelError);
  CHECK(vr.passed);
  CHECK(vr.maxRelError < 1e-3);
  return REPORT();
}

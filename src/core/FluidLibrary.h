#pragma once
#include <string>
#include <vector>

namespace umpnap {

// Constant-property fluid model (Phase 1). SI units.
struct FluidProps {
  double density;       // kg/m^3
  double viscosity;     // Pa.s (dynamic)
  double cp;            // J/(kg.K)
  double conductivity;  // W/(m.K)
};

class FluidLibrary {
 public:
  // Ordered list of all available fluid names (GUI populates from this).
  static const std::vector<std::string>& names();
  // Liquids only / gases only (for medium-appropriate selectors).
  static const std::vector<std::string>& liquids();
  static const std::vector<std::string>& gases();
  static bool isGas(const std::string& name);
  // Properties for a fluid. Falls back to Light Water if unknown.
  static FluidProps props(const std::string& name);
  static bool has(const std::string& name);
};

}  // namespace umpnap

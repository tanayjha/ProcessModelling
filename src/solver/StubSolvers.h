#pragma once
#include "solver/ISolver.h"

namespace umpnap {

// Registered placeholders so the architecture supports all domains. These do
// not solve yet (Phases 2-4); they report "not implemented" cleanly.
class GasSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Gas; }
  SolveReport solveSteady(Network&, Results&, double) override {
    return {false, 0, 0.0, "Gas solver not implemented (Phase 3)."};
  }
};

class ThermalSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Thermal; }
  SolveReport solveSteady(Network&, Results&, double) override {
    return {false, 0, 0.0, "Thermal solver not implemented (Phase 2)."};
  }
};

class ElectricalSolver : public ISolver {
 public:
  Domain domain() const override { return Domain::Electrical; }
  SolveReport solveSteady(Network&, Results&, double) override {
    return {false, 0, 0.0, "Electrical solver not implemented."};
  }
};

}  // namespace umpnap

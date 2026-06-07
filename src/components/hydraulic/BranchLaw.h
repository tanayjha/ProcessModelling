#pragma once
#include <string>

#include "core/Component.h"
#include "core/FluidLibrary.h"

namespace umpnap {

// Result of evaluating a branch: volumetric flow (in->out, m^3/s) at the given
// pressure drop, plus the derivative dQ/d(dP) used for the Newton Jacobian.
struct BranchEval {
  double Q = 0.0;
  double dQ_ddP = 0.0;
};

// True for two-port flow elements (Pipe, Valve, Orifice, Pump, HeatExchanger,
// and the pneumatic/filter aliases). Boundary/Tank/Junction are nodes.
bool isBranch(const std::string& type);

// True for inventory-bearing vessels (Tank, PressurizedTank, AirReceiver) that
// pin a node pressure from their liquid level.
bool isTankType(const std::string& type);

// dP = P_in - P_out (Pa). Returns flow from in to out.
BranchEval evalBranch(const Component& c, double dP, const FluidProps& f);

// Common quadratic-resistance branch: dP = K*Q*|Q|  =>  Q = sign(dP)*sqrt(|dP|/K).
// Linearised for |dP| < eps so the Jacobian stays finite near zero flow.
BranchEval resistanceFlow(double dP, double K, double eps = 1.0);

// Darcy friction factor: laminar 64/Re below Re=2300, else Swamee-Jain.
double frictionFactor(double Re, double relRoughness);

}  // namespace umpnap

#include "components/hydraulic/BranchLaw.h"

#include <cmath>

namespace umpnap {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kG = 9.80665;  // m/s^2
}  // namespace

bool isBranch(const std::string& type) {
  return type == "Pipe" || type == "Valve" || type == "Orifice" ||
         type == "Pump" || type == "HeatExchanger";
}

BranchEval resistanceFlow(double dP, double K, double eps) {
  BranchEval r;
  if (K <= 0.0) return r;  // no resistance defined -> no flow contribution
  double adp = std::fabs(dP);
  if (adp < eps) {
    // Linear region: Q = m*dP, chosen continuous with sqrt branch at |dP|=eps.
    double m = 1.0 / std::sqrt(K * eps);
    r.Q = m * dP;
    r.dQ_ddP = m;
    return r;
  }
  double q = std::sqrt(adp / K);
  r.Q = (dP >= 0.0 ? q : -q);
  r.dQ_ddP = 1.0 / (2.0 * std::sqrt(K * adp));
  return r;
}

double frictionFactor(double Re, double relRoughness) {
  if (Re < 1.0) Re = 1.0;
  if (Re < 2300.0) return 64.0 / Re;
  // Swamee-Jain explicit approximation of Colebrook.
  double t = relRoughness / 3.7 + 5.74 / std::pow(Re, 0.9);
  double denom = std::log10(t);
  return 0.25 / (denom * denom);
}

namespace {

// Pipe resistance coefficient K such that dP = K*Q*|Q|, evaluated at the current
// pressure drop (friction depends on Re, which depends on Q -> the outer Newton
// loop re-evaluates this each iteration as dP changes).
double pipeK(const Component& c, double dP, const FluidProps& f) {
  double L = c.param("length");
  double D = c.param("diameter");
  double rough = c.param("roughness");
  if (D <= 0.0) return 0.0;
  double A = kPi * D * D / 4.0;
  // Estimate velocity/Re from a provisional turbulent guess to pick f.
  // Use f from current dP estimate of Q with a fixed-point-friendly seed.
  double fGuess = 0.02;
  double K = fGuess * (L / D) * f.density / (2.0 * A * A);
  // One refinement: estimate Q from current dP, recompute Re and f.
  double q = std::sqrt(std::fabs(dP) / (K > 0 ? K : 1.0));
  double v = q / A;
  double Re = f.density * v * D / f.viscosity;
  double fr = frictionFactor(Re, rough / D);
  return fr * (L / D) * f.density / (2.0 * A * A);
}

double valveK(const Component& c, const FluidProps& f) {
  double Kv = c.param("Kv");
  double pos = c.param("position");
  if (pos < 1e-3) pos = 1e-3;  // nearly closed -> very high resistance
  return (Kv / (pos * pos)) * f.density;
}

double orificeK(const Component& c, const FluidProps& f) {
  double bore = c.param("bore");
  double Cd = c.param("Cd");
  double A = kPi * bore * bore / 4.0;
  if (A <= 0.0 || Cd <= 0.0) return 0.0;
  return f.density / (2.0 * Cd * Cd * A * A);
}

double hxK(const Component& c, const FluidProps& f) {
  return c.param("K_hx") * f.density;
}

// Pump: pressure rise ρg(H0 - a Q^2) = (P_out - P_in) = -dP.
// => Q = sqrt(max((H0 + dP/(ρg)) / a, 0)).
BranchEval pumpEval(const Component& c, double dP, const FluidProps& f) {
  BranchEval r;
  double H0 = c.param("H0");
  double a = c.param("a");
  double rg = f.density * kG;
  if (a <= 0.0 || rg <= 0.0) return r;
  double u = (H0 + dP / rg) / a;  // = Q^2
  if (u <= 0.0) {
    r.Q = 0.0;
    r.dQ_ddP = 1e-9;  // tiny slope keeps Jacobian non-singular
    return r;
  }
  double q = std::sqrt(u);
  r.Q = q;
  // dQ/ddP = (1/(2 sqrt(u))) * d u/ddP, du/ddP = 1/(rg*a).
  r.dQ_ddP = (1.0 / (2.0 * q)) * (1.0 / (rg * a));
  return r;
}

}  // namespace

BranchEval evalBranch(const Component& c, double dP, const FluidProps& f) {
  if (c.type == "Pump") return pumpEval(c, dP, f);
  double K = 0.0;
  if (c.type == "Pipe")
    K = pipeK(c, dP, f);
  else if (c.type == "Valve")
    K = valveK(c, f);
  else if (c.type == "Orifice")
    K = orificeK(c, f);
  else if (c.type == "HeatExchanger")
    K = hxK(c, f);
  return resistanceFlow(dP, K);
}

}  // namespace umpnap

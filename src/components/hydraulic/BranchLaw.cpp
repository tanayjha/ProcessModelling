// ============================================================================
//  UMPNAP hydraulic component models  --  branch constitutive laws
// ----------------------------------------------------------------------------
//  A "branch" is a two-port flow element. Given the pressure drop across it,
//  dP = P_in - P_out  [Pa], each model returns the volumetric flow Q [m^3/s]
//  from inlet to outlet, together with dQ/d(dP) for the Newton-Raphson Jacobian.
//
//  Sign convention:  dP > 0  =>  flow from in -> out  =>  Q > 0.
//
//  Most passive elements are quadratic resistances of the form
//        dP = K * Q * |Q|            (turbulent, inertia-dominated loss)
//  so that   Q = sign(dP) * sqrt(|dP| / K).
//  The only difference between pipe / valve / orifice / heat-exchanger is HOW
//  the resistance coefficient K is computed from the design data. The pump is
//  active and instead imposes a head-vs-flow characteristic.
//
//  Every model below documents its governing equation and a reference.
// ============================================================================
#include "components/hydraulic/BranchLaw.h"

#include <algorithm>
#include <cmath>

#include "core/CurveFit.h"

namespace umpnap {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kG = 9.80665;  // standard gravity [m/s^2]
}  // namespace

bool isBranch(const std::string& type) {
  // Hydraulic primitives plus pneumatic/filter aliases that reuse the same laws.
  return type == "Pipe" || type == "Valve" || type == "Orifice" ||
         type == "Pump" || type == "HeatExchanger" || type == "Duct" ||
         type == "Damper" || type == "Fan" || type == "Blower" ||
         type == "Compressor" || type == "Filter" || type == "Strainer" ||
         type == "ReliefValve" || type == "GasReliefValve";
}

bool isTankType(const std::string& type) {
  // Inventory-bearing vessels that pin a node pressure from their level.
  return type == "Tank" || type == "PressurizedTank" || type == "AirReceiver";
}

// ----------------------------------------------------------------------------
//  Generic quadratic-resistance branch:  dP = K * Q * |Q|.
//  Inverted:  Q = sign(dP) * sqrt(|dP| / K),   dQ/d(dP) = 1 / (2*sqrt(K*|dP|)).
//  Near dP = 0 the derivative -> infinity, so a small linear core (|dP| < eps)
//  is used to keep the Jacobian finite and the Newton solve well-conditioned.
// ----------------------------------------------------------------------------
BranchEval resistanceFlow(double dP, double K, double eps) {
  BranchEval r;
  if (K <= 0.0) return r;  // undefined resistance -> contributes no flow
  double adp = std::fabs(dP);
  if (adp < eps) {
    // Continuous linear core: Q = m*dP with m chosen so Q matches the sqrt
    // branch in magnitude at |dP| = eps.
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

// ----------------------------------------------------------------------------
//  Darcy friction factor f.
//    Laminar  (Re < 2300):     f = 64 / Re                 (Hagen-Poiseuille)
//    Turbulent:                Colebrook-White, approximated explicitly by the
//                              Swamee-Jain correlation:
//        f = 0.25 / [ log10( e/(3.7 D) + 5.74 / Re^0.9 ) ]^2
//  Ref: Swamee & Jain (1976); White, "Fluid Mechanics".
// ----------------------------------------------------------------------------
double frictionFactor(double Re, double relRoughness) {
  if (Re < 1.0) Re = 1.0;
  if (Re < 2300.0) return 64.0 / Re;
  double t = relRoughness / 3.7 + 5.74 / std::pow(Re, 0.9);
  double denom = std::log10(t);
  return 0.25 / (denom * denom);
}

namespace {

// ---- PIPE -----------------------------------------------------------------
//  Darcy-Weisbach pressure drop with additional lumped minor (fitting) losses:
//        dP = ( f * L/D + sum_K_minor ) * (rho * v^2 / 2) * tuning
//  with mean velocity v = Q / A,  A = pi D^2 / 4  (D = inner diameter ID).
//  Written as dP = K * Q*|Q| gives
//        K = ( f*L/D + Kminor ) * rho / (2 A^2) * tuning.
//  Because f depends on Re(Q), K is re-evaluated at the current dP each Newton
//  iteration (the outer loop converges f and Q together).
//  Ref: Darcy-Weisbach equation; Crane TP-410 for minor-loss K factors.
double pipeK(const Component& c, double dP, const FluidProps& f) {
  double L = c.param("length");
  double D = c.param("ID");  // inner diameter
  double rough = c.param("roughness");
  double Kminor = c.param("minorK");
  double tuning = c.param("tuning");
  if (tuning <= 0.0) tuning = 1.0;
  if (D <= 0.0) return 0.0;
  double A = kPi * D * D / 4.0;

  // Provisional flow estimate from the current dP to evaluate Re, then f.
  double Kseed = 0.02 * (L / D) * f.density / (2.0 * A * A);
  double q = std::sqrt(std::fabs(dP) / (Kseed > 0 ? Kseed : 1.0));
  double v = q / A;
  double Re = f.density * v * D / f.viscosity;
  double fr = frictionFactor(Re, rough / D);
  return (fr * L / D + Kminor) * f.density / (2.0 * A * A) * tuning;
}

// ---- VALVE ----------------------------------------------------------------
//  Control-valve sizing by flow coefficient Kv (metric).  Definition:
//        Q[m^3/h] = Kv * sqrt( dP[bar] / SG ),   SG = rho/rho_water.
//  The installed coefficient depends on travel via the inherent characteristic
//  phi(position):
//        linear:           phi = x
//        equal-percentage: phi = R^(x-1)          (R = rangeability)
//        quick-opening:    phi = sqrt(x)
//  so Kv_eff = Kv_rated * phi(position). Converting to SI and the K form
//  dP = K * Q*|Q| (with dP in Pa, Q in m^3/s):
//        K = (1e5 * rho/1000) * (3600 / Kv_eff)^2
//  Ref: IEC 60534-2-1 (valve sizing); ISA control-valve handbook.
double valveK(const Component& c, const FluidProps& f) {
  double Kv = c.param("Kv");
  double x = std::clamp(c.param("position"), 0.0, 1.0);
  double R = c.param("rangeability");
  if (R < 1.1) R = 50.0;
  int charType = (int)std::lround(c.param("characteristic"));

  double phi;
  switch (charType) {
    case 0: phi = x; break;                       // linear
    case 2: phi = std::sqrt(x); break;            // quick-opening
    default: phi = std::pow(R, x - 1.0); break;   // equal-percentage
  }
  double Kv_eff = Kv * phi;
  if (Kv_eff < 1e-6) Kv_eff = 1e-6;  // essentially closed -> huge resistance
  double ratio = 3600.0 / Kv_eff;
  return (1.0e5 * f.density / 1000.0) * ratio * ratio;
}

// ---- ORIFICE (metering plate) --------------------------------------------
//  ISO 5167 thin-plate orifice. With bore d, pipe bore D, beta = d/D:
//        Q = (Cd / sqrt(1 - beta^4)) * A_o * sqrt( 2 dP / rho )
//  with A_o = pi d^2 / 4. Inverting to dP = K Q|Q|:
//        K = (rho / 2) * (1 - beta^4) / (Cd^2 * A_o^2)
//  Ref: ISO 5167-2; Bernoulli with discharge coefficient.
double orificeK(const Component& c, const FluidProps& f) {
  double d = c.param("bore");
  double D = c.param("pipeID");
  double Cd = c.param("Cd");
  double Ao = kPi * d * d / 4.0;
  if (Ao <= 0.0 || Cd <= 0.0) return 0.0;
  double beta = (D > 0.0) ? d / D : 0.0;
  double oneMinusB4 = 1.0 - beta * beta * beta * beta;
  if (oneMinusB4 < 0.05) oneMinusB4 = 0.05;  // guard for beta -> 1
  return (f.density / 2.0) * oneMinusB4 / (Cd * Cd * Ao * Ao);
}

// ---- HEAT EXCHANGER (tube side, hydraulic) --------------------------------
//  Shell-and-tube, tube-side pressure drop. Flow Q splits equally among
//  N tubes, each carrying q = Q/N through bore d_i with effective length
//  L_eff = L_tube * n_passes:
//        dP = ( f * L_eff/d_i + Kminor ) * rho * v_t^2 / 2,  v_t = q / A_t
//  Written as dP = K Q|Q| with A_t = pi d_i^2/4:
//        K = ( f*L_eff/d_i + Kminor ) * rho / (2 (N A_t)^2)
//  The thermal duty (not solved in Phase 1) would be  Qdot = U * A_s * LMTD,
//  with surface area A_s = N * pi * d_o * L_tube.
//  Ref: tube-side dP (Kern); LMTD method for Qdot.
// ---- FILTER / STRAINER ----------------------------------------------------
//  Fixed quadratic resistance from a datasheet clean-element point: a pressure
//  drop `ratedDP` at flow `ratedFlow`.  K = ratedDP / ratedFlow^2.
//  Ref: manufacturer clean-element ΔP curve (fouling not modelled here).
double filterK(const Component& c) {
  double Qr = c.param("ratedFlow");
  double dPr = c.param("ratedDP");
  if (Qr <= 0.0) return 0.0;
  return dPr / (Qr * Qr);
}

// ---- RELIEF / SAFETY VALVE ------------------------------------------------
//  Self-acting pressure relief. Lift fraction ramps linearly from 0 to 1 as the
//  differential dP rises from `setpoint` to `setpoint + blowdown`:
//        phi = clamp( (dP - setpoint) / blowdown, 0, 1 )
//  The installed coefficient is Kv_eff = Kv * phi, then the usual Kv->K form
//  (as for a control valve) gives the quadratic resistance. Because phi
//  depends on dP it is re-evaluated every Newton iteration. dP <= setpoint (or
//  reverse flow) gives phi = 0 -> essentially closed (one-way action).
//  Ref: API 520/526 relief-valve sizing; valve Kv sizing (IEC 60534-2-1).
double reliefK(const Component& c, double dP, const FluidProps& f) {
  double set = c.param("setpoint");
  double band = c.param("blowdown");
  if (band < 1.0) band = 1.0;
  double Kv = c.param("Kv");
  double phi = std::clamp((dP - set) / band, 0.0, 1.0);
  double Kv_eff = Kv * phi;
  if (Kv_eff < 1e-6) Kv_eff = 1e-6;  // shut -> very large resistance
  double ratio = 3600.0 / Kv_eff;
  return (1.0e5 * f.density / 1000.0) * ratio * ratio;
}

double hxK(const Component& c, double dP, const FluidProps& f) {
  double L = c.param("tubeLength");
  double di = c.param("tubeID");
  double N = std::max(1.0, c.param("numTubes"));
  double passes = std::max(1.0, c.param("numPasses"));
  double rough = c.param("roughness");
  double Kminor = c.param("minorK");
  if (di <= 0.0) return 0.0;
  double At = kPi * di * di / 4.0;
  double Leff = L * passes;

  // Estimate per-tube Re from current dP to pick f.
  double Kseed = 0.02 * (Leff / di) * f.density / (2.0 * (N * At) * (N * At));
  double q = std::sqrt(std::fabs(dP) / (Kseed > 0 ? Kseed : 1.0));
  double vt = (q / N) / At;
  double Re = f.density * vt * di / f.viscosity;
  double fr = frictionFactor(Re, rough / di);
  return (fr * Leff / di + Kminor) * f.density / (2.0 * (N * At) * (N * At));
}

// ---- PUMP -----------------------------------------------------------------
//  Centrifugal pump head-vs-flow characteristic  H(Q)  [m].
//  Two sources of the curve, in priority order:
//   1. If the component carries a "head" curve (>=3 points of (Q,H)), a
//      least-squares quadratic is fitted:  H(Q) = a0 + a1 Q + a2 Q^2.
//   2. Otherwise it is built from datasheet scalars: shutoff head H0 at Q=0
//      and rated head Hr at rated flow Qr, as  H(Q) = H0 + ((Hr-H0)/Qr^2) Q^2
//      (i.e. a1 = 0, a2 = (Hr-H0)/Qr^2 < 0).
//  Variable-speed operation uses the affinity laws (Q ~ s, H ~ s^2). For a
//  quadratic this maps to:  a0 -> s^2 a0,  a1 -> s a1,  a2 -> a2.
//
//  The pump raises outlet pressure, so dP = P_in - P_out = -rho g H(Q).
//  Given dP we solve  H(Q) = -dP/(rho g) = H_t  for the operating flow:
//        a2 Q^2 + a1 Q + (a0 - H_t) = 0.
//  dQ/d(dP) follows by implicit differentiation:
//        (2 a2 Q + a1) Q' = dH_t/d(dP) = -1/(rho g).
//  Ref: pump affinity laws; manufacturer characteristic curves.
void pumpCoeffs(const Component& c, double& a0, double& a1, double& a2) {
  const auto* curve = c.curve("head");
  std::vector<double> fit;
  if (curve && curve->size() >= 3) fit = polyFit(*curve, 2);
  if (fit.size() == 3) {
    a0 = fit[0]; a1 = fit[1]; a2 = fit[2];
  } else {
    double H0 = c.param("shutoffHead");
    double Hr = c.param("ratedHead");
    double Qr = c.param("ratedFlow");
    a0 = H0;
    a1 = 0.0;
    a2 = (Qr > 1e-9) ? (Hr - H0) / (Qr * Qr) : -1.0;
  }
  // Apply affinity scaling for the commanded speed ratio s.
  double s = c.param("speedRatio");
  if (s <= 0.0) s = 1.0;
  a0 *= s * s;
  a1 *= s;
  // a2 unchanged under the affinity transform.
}

BranchEval pumpEval(const Component& c, double dP, const FluidProps& f) {
  BranchEval r;
  double a0, a1, a2;
  pumpCoeffs(c, a0, a1, a2);
  double rg = f.density * kG;
  if (rg <= 0.0 || a2 == 0.0) return r;

  double Ht = -dP / rg;  // required head for this pressure rise
  // Solve a2 Q^2 + a1 Q + (a0 - Ht) = 0 for the physical (Q >= 0) root.
  double cc = a0 - Ht;
  double disc = a1 * a1 - 4.0 * a2 * cc;
  if (disc < 0.0) {
    // Operating point above shutoff (more head demanded than available):
    // clamp to zero flow with a tiny slope to keep the Jacobian non-singular.
    r.Q = 0.0;
    r.dQ_ddP = 1e-9;
    return r;
  }
  double sq = std::sqrt(disc);
  double q1 = (-a1 + sq) / (2.0 * a2);
  double q2 = (-a1 - sq) / (2.0 * a2);
  // Choose the non-negative root on the operating branch (smaller Q wins when
  // both are positive -- the stable left side of the down-opening parabola).
  double Q = -1.0;
  for (double cand : {q1, q2})
    if (cand >= 0.0 && (Q < 0.0 || cand < Q)) Q = cand;
  if (Q < 0.0) { r.Q = 0.0; r.dQ_ddP = 1e-9; return r; }

  double slope = 2.0 * a2 * Q + a1;  // dH/dQ at the operating point
  r.Q = Q;
  r.dQ_ddP = (std::fabs(slope) > 1e-12) ? (-1.0 / (slope * rg)) : 1e-9;
  return r;
}

}  // namespace

BranchEval evalBranch(const Component& c, double dP, const FluidProps& f) {
  // Active pressure-rise devices (pump and its pneumatic cousins).
  if (c.type == "Pump" || c.type == "Fan" || c.type == "Blower" ||
      c.type == "Compressor")
    return pumpEval(c, dP, f);
  // Pipe/duct include a static-head term from the elevation change dZ
  // (= outlet elevation - inlet elevation):
  //   P_in - P_out = K*Q|Q| + rho*g*dZ   =>   Q = f( (dP - rho*g*dZ), K ).
  if (c.type == "Pipe" || c.type == "Duct") {
    double K = pipeK(c, dP, f);
    double dPeff = dP - f.density * kG * c.param("dZ");
    return resistanceFlow(dPeff, K);
  }
  // Relief/safety valves open on the pressure differential across them.
  if (c.type == "ReliefValve" || c.type == "GasReliefValve")
    return resistanceFlow(dP, reliefK(c, dP, f));
  double K = 0.0;
  if (c.type == "Valve" || c.type == "Damper")
    K = valveK(c, f);
  else if (c.type == "Orifice")
    K = orificeK(c, f);
  else if (c.type == "HeatExchanger")
    K = hxK(c, dP, f);
  else if (c.type == "Filter" || c.type == "Strainer")
    K = filterK(c);
  return resistanceFlow(dP, K);
}

}  // namespace umpnap

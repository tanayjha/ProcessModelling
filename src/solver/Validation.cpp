#include "solver/Validation.h"

#include <cmath>
#include <cstdio>

#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"
#include "core/Network.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"
#include "solver/SolverManager.h"

namespace umpnap {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Same K as BranchLaw's orifice law, recomputed for the analytical reference.
double orificeKref(double bore, double Cd, double rho) {
  double A = kPi * bore * bore / 4.0;
  return rho / (2.0 * Cd * Cd * A * A);
}
}  // namespace

ValidationResult runPipeNetworkValidation(double tol) {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  Network net;
  // Boundaries.
  auto bA = reg.create("Boundary");
  bA->params["pressure"] = 3.0e5;
  int idA = net.addComponent(std::move(bA));
  auto bB = reg.create("Boundary");
  bB->params["pressure"] = 1.0e5;
  int idB = net.addComponent(std::move(bB));

  // Four orifices (all defaults: bore 0.05, Cd 0.62, Light Water).
  int o1 = net.addComponent(reg.create("Orifice"));
  int o2 = net.addComponent(reg.create("Orifice"));
  int o3 = net.addComponent(reg.create("Orifice"));
  int o4 = net.addComponent(reg.create("Orifice"));

  // Topology:  A -O1- M -(O2 || O3)- N -O4- B
  net.connect(idA, "p", o1, "in");
  net.connect(o1, "out", o2, "in");   // node M = {O1.out, O2.in, O3.in}
  net.connect(o1, "out", o3, "in");
  net.connect(o2, "out", o4, "in");   // node N = {O2.out, O3.out, O4.in}
  net.connect(o3, "out", o4, "in");
  net.connect(o4, "out", idB, "p");

  Results res;
  SolverManager mgr;
  SolveReport rep = mgr.runSteady(net, res);

  // Analytical solution.
  FluidProps f = FluidLibrary::props("Light Water");
  double K = orificeKref(0.05, 0.62, f.density);
  double Kpar = K / 4.0;  // two identical parallel orifices
  double Qa = std::sqrt(8.0e5 / (9.0 * K));
  double PMa = 3.0e5 - K * Qa * Qa;
  double PNa = 1.0e5 + K * Qa * Qa;

  // Solver values.
  NodeGraph g = buildNodeGraph(net);
  int nodeM = g.nodeOf(o1, "out");
  int nodeN = g.nodeOf(o4, "in");
  double PMs = res.latest("node." + std::to_string(nodeM) + ".pressure");
  double PNs = res.latest("node." + std::to_string(nodeN) + ".pressure");
  double Qs = res.latest("comp." + std::to_string(o1) + ".flow");

  auto rel = [](double a, double b) {
    double d = std::fabs(a - b);
    double scale = std::fabs(b) > 1e-9 ? std::fabs(b) : 1.0;
    return d / scale;
  };

  double eQ = rel(Qs, Qa);
  double ePM = rel(PMs, PMa);
  double ePN = rel(PNs, PNa);
  double maxErr = std::max(eQ, std::max(ePM, ePN));
  (void)Kpar;

  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "converged=%d iters=%d | Q: solver=%.6g analytic=%.6g (e=%.2e) | "
                "P_M: %.6g vs %.6g (e=%.2e) | P_N: %.6g vs %.6g (e=%.2e)",
                rep.converged ? 1 : 0, rep.iterations, Qs, Qa, eQ, PMs, PMa, ePM,
                PNs, PNa, ePN);

  ValidationResult vr;
  vr.maxRelError = maxErr;
  vr.passed = rep.converged && maxErr < tol;
  vr.detail = buf;
  return vr;
}

}  // namespace umpnap

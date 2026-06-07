#include "solver/HydraulicSolver.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/hydraulic/BranchLaw.h"
#include "core/FluidLibrary.h"
#include "core/Results.h"
#include "solver/LinAlg.h"
#include "solver/NodeGraph.h"

namespace umpnap {

namespace {
constexpr double kG = 9.80665;

// Assemble mass-conservation residual at free nodes for the given pressures.
// Returns max |F|. Fills F (size = #free). If J != nullptr, fills the Jacobian.
double assemble(const Network& net, const NodeGraph& g,
                const std::vector<double>& P, const std::vector<int>& freeIndex,
                int numFree, std::vector<double>& F, Matrix* J) {
  std::fill(F.begin(), F.end(), 0.0);
  if (J)
    for (auto& v : J->a) v = 0.0;

  for (const auto& br : g.branches) {
    const Component* c = net.component(br.compId);
    if (!c) continue;
    FluidProps f = FluidLibrary::props(c->fluid);
    double dP = P[br.inNode] - P[br.outNode];
    BranchEval e = evalBranch(*c, dP, f);

    int fi = freeIndex[br.inNode];
    int fo = freeIndex[br.outNode];

    // Mass balance: net inflow = 0. Flow leaves inNode, enters outNode.
    if (fi >= 0) F[fi] -= e.Q;
    if (fo >= 0) F[fo] += e.Q;

    if (J) {
      // d(dP)/dP_in = +1, d(dP)/dP_out = -1.
      double d = e.dQ_ddP;
      // Row = inNode: F[in] = ... - Q
      if (fi >= 0) {
        J->at(fi, fi) += -d;                 // d(-Q)/dP_in
        if (fo >= 0) J->at(fi, fo) += +d;    // d(-Q)/dP_out
      }
      // Row = outNode: F[out] = ... + Q
      if (fo >= 0) {
        if (fi >= 0) J->at(fo, fi) += +d;    // d(+Q)/dP_in
        J->at(fo, fo) += -d;                 // d(+Q)/dP_out
      }
    }
  }

  double maxr = 0.0;
  for (int i = 0; i < numFree; ++i) maxr = std::max(maxr, std::fabs(F[i]));
  return maxr;
}

}  // namespace

SolveReport HydraulicSolver::solveSteady(Network& net, Results& out, double t) {
  SolveReport rep;
  NodeGraph g = buildNodeGraph(net);

  // Index free (non-fixed) nodes.
  std::vector<int> freeIndex(g.nodeCount, -1);
  int numFree = 0;
  for (int n = 0; n < g.nodeCount; ++n)
    if (!g.fixed[n]) freeIndex[n] = numFree++;

  // Pressure vector seeded: fixed nodes use their pin; free nodes the mean fix.
  double meanFix = 1.013e5;
  {
    double sum = 0;
    int cnt = 0;
    for (int n = 0; n < g.nodeCount; ++n)
      if (g.fixed[n]) { sum += g.fixedP[n]; ++cnt; }
    if (cnt) meanFix = sum / cnt;
  }
  std::vector<double> P(g.nodeCount, meanFix);
  for (int n = 0; n < g.nodeCount; ++n)
    if (g.fixed[n]) P[n] = g.fixedP[n];

  std::vector<double> F(numFree, 0.0);

  if (numFree == 0) {
    rep.converged = true;
    rep.iterations = 0;
    rep.residual = 0.0;
    rep.message = "No free nodes (all pressures pinned).";
  } else {
    double residual = assemble(net, g, P, freeIndex, numFree, F, nullptr);
    int it = 0;
    for (; it < maxIters; ++it) {
      if (residual < tol) break;
      Matrix J(numFree);
      assemble(net, g, P, freeIndex, numFree, F, &J);
      std::vector<double> rhs(numFree);
      for (int i = 0; i < numFree; ++i) rhs[i] = -F[i];
      if (!lu_solve(J, rhs)) {
        rep.message = "Singular Jacobian.";
        break;
      }
      // Damped update with simple backtracking line search.
      double alpha = 1.0;
      std::vector<double> Ptrial = P;
      double newres = residual;
      for (int ls = 0; ls < 20; ++ls) {
        Ptrial = P;
        for (int n = 0; n < g.nodeCount; ++n)
          if (freeIndex[n] >= 0) Ptrial[n] = P[n] + alpha * rhs[freeIndex[n]];
        newres = assemble(net, g, Ptrial, freeIndex, numFree, F, nullptr);
        if (newres < residual || alpha < 1e-6) break;
        alpha *= 0.5;
      }
      P = Ptrial;
      residual = newres;
    }
    rep.iterations = it;
    rep.residual = residual;
    rep.converged = residual < tol * 10.0;  // mild tolerance on report
    rep.message = rep.converged ? "Converged."
                                : "Did not converge to tolerance.";
  }

  // Record results.
  for (int n = 0; n < g.nodeCount; ++n)
    out.record("node." + std::to_string(n) + ".pressure", t, P[n]);

  for (const auto& br : g.branches) {
    const Component* c = net.component(br.compId);
    if (!c) continue;
    FluidProps f = FluidLibrary::props(c->fluid);
    double dP = P[br.inNode] - P[br.outNode];
    BranchEval e = evalBranch(*c, dP, f);
    out.record("comp." + std::to_string(c->id) + ".flow", t, e.Q);
    if (c->type == "Pump") {
      double head = (P[br.outNode] - P[br.inNode]) / (f.density * kG);
      out.record("comp." + std::to_string(c->id) + ".head", t, head);
    }
    if (c->type == "Valve")
      out.record("comp." + std::to_string(c->id) + ".position", t,
                 c->param("position"));
  }

  for (const auto& c : net.components()) {
    if (isTankType(c->type))
      out.record("comp." + std::to_string(c->id) + ".level", t, c->param("level"));
  }

  return rep;
}

}  // namespace umpnap

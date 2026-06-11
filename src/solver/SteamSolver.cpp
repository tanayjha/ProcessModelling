#include "solver/SteamSolver.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/Component.h"
#include "core/Results.h"
#include "solver/LinAlg.h"

namespace umpnap {

namespace {

// Steam reference state for the isothermal density (superheated, ~300 C).
constexpr double kSteamRs = 461.5;       // J/(kg.K), water vapour gas constant
constexpr double kSteamT = 273.15 + 300; // K, reference temperature
constexpr double kTurbHDrop = 1.0e6;     // J/kg, nominal turbine enthalpy drop
constexpr double kSteamValveC = 1.0e-7;  // (kg/s)/Pa per unit Kv (calibration)

bool isSteamSource(const std::string& t) {
  return t == "SteamGenerator";
}
bool isSteamSink(const std::string& t) {
  return t == "Condenser" || t == "Deaerator";
}
bool isSteamBranch(const std::string& t) {
  return t == "Turbine" || t == "ASDV" || t == "CSDV";
}

// Per-component swallowing coefficient C in  mdot = C*sign(s)*sqrt(|s|).
double branchC(const Component& c) {
  if (c.type == "Turbine") {
    double pin = c.param("inletP");
    double pex = c.param("exhaustP");
    double s = pin * pin - pex * pex;
    if (s <= 0.0) return 0.0;
    double mdotRated = c.param("ratedPower") * 1.0e6 / kTurbHDrop;  // MW->W
    return mdotRated / std::sqrt(s);
  }
  // Steam dump / relief valve: opening is the larger of the commanded position
  // and the self-acting lift once the inlet exceeds the setpoint.
  double phi = std::clamp(c.param("position"), 0.0, 1.0);
  return kSteamValveC * c.param("Kv") * std::max(phi, 0.0);
}

struct Flow {
  double mdot = 0.0, dP1 = 0.0, dP2 = 0.0;
};
// mdot = C*sign(s)*sqrt(|s|), s = P1^2 - P2^2, linearised for |s| < eps.
Flow compressibleFlow(double P1, double P2, double C) {
  Flow r;
  if (C <= 0.0) return r;
  double s = P1 * P1 - P2 * P2;
  const double eps = 1.0e7;  // Pa^2 linear core around zero differential
  if (std::fabs(s) < eps) {
    double m = C / std::sqrt(eps);
    r.mdot = m * s;
    r.dP1 = m * 2.0 * P1;
    r.dP2 = -m * 2.0 * P2;
    return r;
  }
  double root = std::sqrt(std::fabs(s));
  r.mdot = (s >= 0.0 ? C * root : -C * root);
  double d = C / (2.0 * root);  // d(mdot)/ds
  r.dP1 = d * 2.0 * P1;
  r.dP2 = -d * 2.0 * P2;
  return r;
}

struct SteamBranch {
  int compId, inNode, outNode;
};

}  // namespace

PortNodeMap buildSteamPortNodes(const Network& net) {
  auto isSteamPort = [&](const Component& c, const Port& p) {
    return effectiveMedium(c, p.name) == Medium::Steam;
  };
  auto participates = [](const std::string& ty) {
    return isSteamSource(ty) || isSteamSink(ty) || isSteamBranch(ty);
  };
  std::map<std::pair<int, std::string>, int> portIndex;
  std::vector<std::pair<int, std::string>> portList;
  for (const auto& c : net.components()) {
    if (!participates(c->type)) continue;
    for (const auto& p : c->ports)
      if (isSteamPort(*c, p)) {
        portIndex[{c->id, p.name}] = (int)portList.size();
        portList.push_back({c->id, p.name});
      }
  }
  std::vector<int> parent(portList.size());
  for (size_t i = 0; i < parent.size(); ++i) parent[i] = (int)i;
  std::function<int(int)> find = [&](int x) {
    while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
    return x;
  };
  auto unite = [&](int a, int b) { parent[find(a)] = find(b); };
  for (const auto& cn : net.connections()) {
    auto a = portIndex.find({cn.compA, cn.portA});
    auto b = portIndex.find({cn.compB, cn.portB});
    if (a != portIndex.end() && b != portIndex.end()) unite(a->second, b->second);
  }
  std::map<int, int> rootToNode;
  auto nodeFor = [&](int pi) {
    int r = find(pi);
    auto it = rootToNode.find(r);
    if (it != rootToNode.end()) return it->second;
    int id = (int)rootToNode.size();
    rootToNode[r] = id;
    return id;
  };
  PortNodeMap m;
  for (size_t i = 0; i < portList.size(); ++i) m.portNode[portList[i]] = nodeFor((int)i);
  m.nodeCount = (int)rootToNode.size();
  return m;
}

SolveReport SteamSolver::solveSteady(Network& net, Results& out, double t) {
  SolveReport rep;

  // ---- Build the steam node graph (union steam ports across connections). ---
  PortNodeMap pn = buildSteamPortNodes(net);
  const auto& portNode = pn.portNode;
  int nodeCount = pn.nodeCount;
  if (portNode.empty()) {
    rep.converged = true;
    rep.message = "No steam network.";
    return rep;
  }

  std::vector<bool> fixed(nodeCount, false);
  std::vector<double> fixedP(nodeCount, 0.0);
  auto pinPort = [&](int compId, const std::string& port, double p) {
    auto it = portNode.find({compId, port});
    if (it == portNode.end()) return;
    fixed[it->second] = true;
    fixedP[it->second] = p;
  };
  for (const auto& c : net.components()) {
    if (isSteamSource(c->type)) pinPort(c->id, "steam", c->param("pressure"));
    else if (isSteamSink(c->type)) pinPort(c->id, "steam", c->param("pressure"));
  }

  std::vector<SteamBranch> branches;
  for (const auto& c : net.components()) {
    if (!isSteamBranch(c->type)) continue;
    std::string inp = (c->type == "Turbine") ? "steam" : "in";
    std::string outp = (c->type == "Turbine") ? "exhaust" : "out";
    auto a = portNode.find({c->id, inp});
    auto b = portNode.find({c->id, outp});
    if (a == portNode.end() || b == portNode.end()) continue;
    branches.push_back({c->id, a->second, b->second});
  }

  // ---- Newton-Raphson on free-node mass balance. ----------------------------
  std::vector<int> freeIndex(nodeCount, -1);
  int numFree = 0;
  for (int n = 0; n < nodeCount; ++n)
    if (!fixed[n]) freeIndex[n] = numFree++;

  double meanFix = 1.0e5;
  {
    double sum = 0; int cnt = 0;
    for (int n = 0; n < nodeCount; ++n) if (fixed[n]) { sum += fixedP[n]; ++cnt; }
    if (cnt) meanFix = sum / cnt;
  }
  std::vector<double> P(nodeCount, meanFix);
  for (int n = 0; n < nodeCount; ++n) if (fixed[n]) P[n] = fixedP[n];

  auto assemble = [&](const std::vector<double>& Pv, std::vector<double>& F,
                      Matrix* J) {
    std::fill(F.begin(), F.end(), 0.0);
    if (J) for (auto& v : J->a) v = 0.0;
    for (const auto& br : branches) {
      const Component* c = net.component(br.compId);
      if (!c) continue;
      Flow e = compressibleFlow(Pv[br.inNode], Pv[br.outNode], branchC(*c));
      int fi = freeIndex[br.inNode], fo = freeIndex[br.outNode];
      if (fi >= 0) F[fi] -= e.mdot;   // mass leaves inNode
      if (fo >= 0) F[fo] += e.mdot;   // mass enters outNode
      if (J) {
        if (fi >= 0) { J->at(fi, fi) -= e.dP1; if (fo >= 0) J->at(fi, fo) -= e.dP2; }
        if (fo >= 0) { if (fi >= 0) J->at(fo, fi) += e.dP1; J->at(fo, fo) += e.dP2; }
      }
    }
    double maxr = 0.0;
    for (int i = 0; i < numFree; ++i) maxr = std::max(maxr, std::fabs(F[i]));
    return maxr;
  };

  std::vector<double> F(numFree, 0.0);
  if (numFree == 0) {
    rep.converged = true;
    rep.message = "Steam: all node pressures pinned.";
  } else {
    double residual = assemble(P, F, nullptr);
    int it = 0;
    for (; it < maxIters; ++it) {
      if (residual < tol) break;
      Matrix J(numFree);
      assemble(P, F, &J);
      std::vector<double> rhs(numFree);
      for (int i = 0; i < numFree; ++i) rhs[i] = -F[i];
      if (!lu_solve(J, rhs)) { rep.message = "Steam: singular Jacobian."; break; }
      double alpha = 1.0, newres = residual;
      std::vector<double> Pt = P;
      for (int ls = 0; ls < 20; ++ls) {
        Pt = P;
        for (int n = 0; n < nodeCount; ++n)
          if (freeIndex[n] >= 0) Pt[n] = std::max(0.0, P[n] + alpha * rhs[freeIndex[n]]);
        newres = assemble(Pt, F, nullptr);
        if (newres < residual || alpha < 1e-6) break;
        alpha *= 0.5;
      }
      P = Pt;
      residual = newres;
    }
    rep.iterations = it;
    rep.residual = residual;
    rep.converged = residual < tol * 10.0;
    rep.message = rep.converged ? "Steam converged." : "Steam did not converge.";
  }

  for (int n = 0; n < nodeCount; ++n)
    out.record("steamnode." + std::to_string(n) + ".pressure", t, P[n]);
  for (const auto& br : branches) {
    const Component* c = net.component(br.compId);
    if (!c) continue;
    Flow e = compressibleFlow(P[br.inNode], P[br.outNode], branchC(*c));
    out.record("comp." + std::to_string(c->id) + ".flow", t, e.mdot);
    if (c->type == "Turbine")
      out.record("comp." + std::to_string(c->id) + ".power", t,
                 std::fabs(e.mdot) * kTurbHDrop * c->param("efficiency"));
  }
  return rep;
}

}  // namespace umpnap

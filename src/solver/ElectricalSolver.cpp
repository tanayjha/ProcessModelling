#include "solver/ElectricalSolver.h"

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
constexpr double kRhoCu = 1.724e-8;   // copper resistivity, ohm.m
constexpr double kLoadVn = 415.0;     // default load nominal voltage, V

double cableR(const Component& c) {
  double L = c.param("length");
  double Amm2 = c.param("area");
  if (Amm2 <= 0.0) return 1e6;
  return kRhoCu * L / (Amm2 * 1.0e-6);  // mm^2 -> m^2
}
}  // namespace

SolveReport ElectricalSolver::solveSteady(Network& net, Results& out, double t) {
  SolveReport rep;

  auto isElecPort = [&](const Component& c, const Port& p) {
    return effectiveMedium(c, p.name) == Medium::Electrical;
  };
  std::map<std::pair<int, std::string>, int> portIndex;
  std::vector<std::pair<int, std::string>> portList;
  for (const auto& c : net.components()) {
    if (c->domain != Domain::Electrical) continue;
    for (const auto& p : c->ports)
      if (isElecPort(*c, p)) {
        portIndex[{c->id, p.name}] = (int)portList.size();
        portList.push_back({c->id, p.name});
      }
  }
  if (portList.empty()) {
    rep.converged = true;
    rep.message = "No electrical network.";
    return rep;
  }

  std::vector<int> parent(portList.size());
  for (size_t i = 0; i < parent.size(); ++i) parent[i] = (int)i;
  std::function<int(int)> find = [&](int x) {
    while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
    return x;
  };
  auto unite = [&](int a, int b) { parent[find(a)] = find(b); };
  auto portIdx = [&](int comp, const std::string& port) -> int {
    auto it = portIndex.find({comp, port});
    return it == portIndex.end() ? -1 : it->second;
  };
  for (const auto& cn : net.connections()) {
    int a = portIdx(cn.compA, cn.portA), b = portIdx(cn.compB, cn.portB);
    if (a >= 0 && b >= 0) unite(a, b);
  }
  // Busbars are ideal nodes; closed breakers are ideal links.
  for (const auto& c : net.components()) {
    if (c->type == "Busbar") {
      int first = -1;
      for (const auto& p : c->ports) {
        int pi = portIdx(c->id, p.name);
        if (pi < 0) continue;
        if (first < 0) first = pi; else unite(first, pi);
      }
    } else if (c->type == "Breaker" && c->param("state") >= 0.5) {
      int a = portIdx(c->id, "in"), b = portIdx(c->id, "out");
      if (a >= 0 && b >= 0) unite(a, b);
    }
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
  std::map<std::pair<int, std::string>, int> portNode;
  for (size_t i = 0; i < portList.size(); ++i) portNode[portList[i]] = nodeFor((int)i);
  auto nodeOf = [&](int comp, const std::string& port) -> int {
    auto it = portNode.find({comp, port});
    return it == portNode.end() ? -1 : it->second;
  };
  int nNodes = (int)rootToNode.size();

  // Enumerate the extra MNA unknowns: voltage sources and ideal transformers.
  struct Src { int node; double v; int comp; };
  struct Tx { int hv, lv; double ratio; int comp; };
  std::vector<Src> sources;
  std::vector<Tx> txs;
  for (const auto& c : net.components()) {
    if (c->type == "Grid" || c->type == "Generator") {
      int n = nodeOf(c->id, "t");
      if (n >= 0) sources.push_back({n, c->param("voltage"), c->id});
    } else if (c->type == "Transformer") {
      int h = nodeOf(c->id, "hv"), l = nodeOf(c->id, "lv");
      double ratio = c->param("ratio");
      if (h >= 0 && l >= 0 && ratio > 0.0) txs.push_back({h, l, ratio, c->id});
    }
  }

  int nExtra = (int)sources.size() + (int)txs.size();
  int m = nNodes + nExtra;
  if (m == 0) { rep.converged = true; rep.message = "Empty electrical network."; return rep; }

  Matrix A(m);
  std::vector<double> z(m, 0.0);
  // Tiny shunt to ground regularises isolated nodes (no source path).
  for (int n = 0; n < nNodes; ++n) A.at(n, n) += 1e-12;

  auto stampG = [&](int a, int b, double g) {  // conductance between a and b
    if (a >= 0) A.at(a, a) += g;
    if (b >= 0) A.at(b, b) += g;
    if (a >= 0 && b >= 0) { A.at(a, b) -= g; A.at(b, a) -= g; }
  };
  // Resistive series branches.
  for (const auto& c : net.components()) {
    if (c->type == "Cable") {
      double R = cableR(*c);
      if (R > 0) stampG(nodeOf(c->id, "in"), nodeOf(c->id, "out"), 1.0 / R);
    }
  }
  // Loads as conductances to ground (G = P / V_rated^2).
  for (const auto& c : net.components()) {
    if (c->type == "Motor") {
      double V = c->param("voltage");
      double eff = c->param("efficiency"); if (eff <= 0) eff = 1.0;
      double Pin = c->param("rating") * 1000.0 / eff;  // kW -> W input
      if (V > 0) stampG(nodeOf(c->id, "t"), -1, Pin / (V * V));
    } else if (c->type == "ElectricalLoad") {
      double P = c->param("power") * 1000.0;  // kW -> W
      stampG(nodeOf(c->id, "t"), -1, P / (kLoadVn * kLoadVn));
    }
  }
  // Voltage sources (terminal to ground).
  int col = nNodes;
  for (const auto& s : sources) {
    A.at(s.node, col) += 1.0;
    A.at(col, s.node) += 1.0;
    z[col] = s.v;
    ++col;
  }
  // Ideal transformers: V_hv - ratio*V_lv = 0.
  for (const auto& tx : txs) {
    A.at(tx.hv, col) += 1.0;
    A.at(tx.lv, col) += -tx.ratio;
    A.at(col, tx.hv) += 1.0;
    A.at(col, tx.lv) += -tx.ratio;
    ++col;
  }

  std::vector<double> x = z;
  if (!lu_solve(A, x)) {
    rep.message = "Electrical: singular system.";
    rep.converged = false;
    return rep;
  }
  rep.converged = true;
  rep.message = "Electrical solved.";

  std::vector<double> V(x.begin(), x.begin() + nNodes);
  for (int n = 0; n < nNodes; ++n)
    out.record("elecnode." + std::to_string(n) + ".voltage", t, V[n]);

  auto Vnode = [&](int comp, const std::string& port) {
    int n = nodeOf(comp, port);
    return n >= 0 ? V[n] : 0.0;
  };
  // Source currents / power. The MNA branch variable is the current flowing
  // back into the source branch; negate so a delivering source reads positive.
  for (size_t k = 0; k < sources.size(); ++k) {
    double i = -x[nNodes + (int)k];
    std::string p = "comp." + std::to_string(sources[k].comp) + ".";
    out.record(p + "voltage", t, V[sources[k].node]);
    out.record(p + "current", t, i);
    out.record(p + "power", t, V[sources[k].node] * i);
  }
  // Transformer primary currents.
  for (size_t k = 0; k < txs.size(); ++k) {
    double i = x[nNodes + (int)sources.size() + (int)k];
    std::string p = "comp." + std::to_string(txs[k].comp) + ".";
    out.record(p + "current", t, i);
    out.record(p + "voltage", t, V[txs[k].hv]);
  }
  // Cables, loads.
  for (const auto& c : net.components()) {
    std::string p = "comp." + std::to_string(c->id) + ".";
    if (c->type == "Cable") {
      double R = cableR(*c);
      double dv = Vnode(c->id, "in") - Vnode(c->id, "out");
      double i = (R > 0) ? dv / R : 0.0;
      out.record(p + "current", t, i);
      out.record(p + "power", t, i * i * R);  // loss
    } else if (c->type == "Motor") {
      double V0 = Vnode(c->id, "t");
      double Vr = c->param("voltage");
      double eff = c->param("efficiency"); if (eff <= 0) eff = 1.0;
      double Pin = c->param("rating") * 1000.0 / eff;
      double G = (Vr > 0) ? Pin / (Vr * Vr) : 0.0;
      out.record(p + "voltage", t, V0);
      out.record(p + "current", t, V0 * G);
      out.record(p + "power", t, V0 * V0 * G);
    } else if (c->type == "ElectricalLoad") {
      double V0 = Vnode(c->id, "t");
      double G = c->param("power") * 1000.0 / (kLoadVn * kLoadVn);
      out.record(p + "voltage", t, V0);
      out.record(p + "current", t, V0 * G);
      out.record(p + "power", t, V0 * V0 * G);
    }
  }
  return rep;
}

}  // namespace umpnap

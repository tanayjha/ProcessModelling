#include "solver/NodeGraph.h"

#include "components/hydraulic/BranchLaw.h"
#include "core/FluidLibrary.h"

namespace umpnap {

namespace {
constexpr double kG = 9.80665;

// Simple union-find over an integer index space.
struct UnionFind {
  std::vector<int> parent;
  void init(int n) {
    parent.resize(n);
    for (int i = 0; i < n; ++i) parent[i] = i;
  }
  int find(int x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  }
  void unite(int a, int b) { parent[find(a)] = find(b); }
};
}  // namespace

NodeGraph buildNodeGraph(const Network& net) {
  NodeGraph g;

  // Only components that participate in the nodal pressure solve form nodes:
  // flow branches, vessels, boundaries, junctions and headers. Instrumentation,
  // electrical and (not-yet-solved) steam equipment are skipped so they cannot
  // create dangling equation-less nodes.
  auto participates = [](const std::string& t) {
    return isBranch(t) || isTankType(t) || t == "Boundary" || t == "Junction" ||
           t == "Header";
  };
  // Only fluid-carrying ports become pressure nodes. Signal/electrical/steam
  // ports (e.g. a valve's actuator tapping or a pump's motor drive) are skipped
  // so attachment links never create dangling equation-less nodes.
  auto fluidPort = [](const Component& c, const Port& p) {
    Medium m = effectiveMedium(c, p.name);
    return m == Medium::Liquid || m == Medium::Gas;
  };
  std::map<std::pair<int, std::string>, int> portIndex;
  std::vector<std::pair<int, std::string>> portList;
  for (const auto& c : net.components()) {
    if (!participates(c->type)) continue;
    for (const auto& p : c->ports) {
      if (!fluidPort(*c, p)) continue;
      portIndex[{c->id, p.name}] = (int)portList.size();
      portList.push_back({c->id, p.name});
    }
  }

  UnionFind uf;
  uf.init((int)portList.size());

  // Union ports joined by connections.
  for (const auto& conn : net.connections()) {
    auto a = portIndex.find({conn.compA, conn.portA});
    auto b = portIndex.find({conn.compB, conn.portB});
    if (a != portIndex.end() && b != portIndex.end()) uf.unite(a->second, b->second);
  }
  // Union all ports of each Junction (ideal zero-drop node) and each
  // AirReceiver (all tappings share one vessel pressure node).
  for (const auto& c : net.components()) {
    if (c->type != "Junction" && c->type != "AirReceiver") continue;
    int first = -1;
    for (const auto& p : c->ports) {
      auto it = portIndex.find({c->id, p.name});
      if (it == portIndex.end()) continue;
      if (first < 0) first = it->second;
      else uf.unite(first, it->second);
    }
  }

  // Map union-find roots to dense node ids.
  std::map<int, int> rootToNode;
  auto nodeFor = [&](int portIdx) {
    int root = uf.find(portIdx);
    auto it = rootToNode.find(root);
    if (it != rootToNode.end()) return it->second;
    int id = g.nodeCount++;
    rootToNode[root] = id;
    return id;
  };

  for (size_t i = 0; i < portList.size(); ++i) {
    int node = nodeFor((int)i);
    g.portNode[portList[i]] = node;
  }

  g.fixed.assign(g.nodeCount, false);
  g.fixedP.assign(g.nodeCount, 0.0);

  // Pin Boundary/Tank node pressures.
  for (const auto& c : net.components()) {
    if (c->type == "Boundary") {
      int n = g.portNode[{c->id, "p"}];
      g.fixed[n] = true;
      g.fixedP[n] = c->param("pressure");
    } else if (isTankType(c->type)) {
      int n = g.portNode[{c->id, "p"}];
      FluidProps f = FluidLibrary::props(c->fluid);
      g.fixed[n] = true;
      g.fixedP[n] = c->param("p_top") + f.density * kG * c->param("level");
      // Cover-gas tapping: pin at the blanket pressure so the gas network sees
      // the vapour space as a fixed-pressure source (and an unconnected tapping
      // does not create a singular free node).
      auto it = g.portNode.find({c->id, "gas"});
      if (it != g.portNode.end()) {
        g.fixed[it->second] = true;
        g.fixedP[it->second] = c->param("p_top");
      }
    }
  }

  // Branches from each two-port flow element.
  for (const auto& c : net.components()) {
    if (!isBranch(c->type)) continue;
    Branch b;
    b.compId = c->id;
    b.inNode = g.portNode[{c->id, "in"}];
    b.outNode = g.portNode[{c->id, "out"}];
    g.branches.push_back(b);
  }

  return g;
}

}  // namespace umpnap

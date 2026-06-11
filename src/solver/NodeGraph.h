#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/Network.h"

namespace umpnap {

// A branch flow element mapped onto solver nodes.
struct Branch {
  int compId = -1;
  int inNode = -1;
  int outNode = -1;
};

// Solver-side topology: ports collapsed into pressure nodes.
struct NodeGraph {
  int nodeCount = 0;
  std::vector<bool> fixed;      // size nodeCount; true if pressure is pinned
  std::vector<double> fixedP;   // pinned pressure (Pa) where fixed
  std::vector<Branch> branches;
  std::map<std::pair<int, std::string>, int> portNode;  // (compId,port) -> node

  int nodeOf(int compId, const std::string& port) const {
    auto it = portNode.find({compId, port});
    return it == portNode.end() ? -1 : it->second;
  }

  // Display label for a node: the '+'-joined P&ID tags of the components whose
  // ports collapse onto it (e.g. "P-101+PIPE-3"). Used so trend signals read by
  // component tag rather than an opaque node index.
  std::string nodeLabel(int node, const Network& net) const;
};

// Builds nodes by unioning connected ports and merging each Junction's ports.
// Boundary/Tank nodes are pinned (Tank pressure = p_top + rho*g*level).
NodeGraph buildNodeGraph(const Network& net);

// Shared label builder over any (compId,port)->node map: the sorted, distinct,
// '+'-joined P&ID tags of the components touching `node`. Returns "" if none map
// there. Used by every domain solver/display so a bus is named by its equipment.
std::string nodeTagLabel(const std::map<std::pair<int, std::string>, int>& portNode,
                         int node, const Network& net);

// A bare port->node clustering (no physics), shared by a domain solver and the
// trend display so both agree on the dense node numbering used to key signals.
struct PortNodeMap {
  std::map<std::pair<int, std::string>, int> portNode;
  int nodeCount = 0;
};

}  // namespace umpnap

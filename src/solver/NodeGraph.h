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
};

// Builds nodes by unioning connected ports and merging each Junction's ports.
// Boundary/Tank nodes are pinned (Tank pressure = p_top + rho*g*level).
NodeGraph buildNodeGraph(const Network& net);

}  // namespace umpnap

#include "check.h"
#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "solver/NodeGraph.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  // Boundary -> Pipe -> Boundary : 2 nodes, both fixed, 1 branch.
  {
    Network net;
    int a = net.addComponent(reg.create("Boundary"));
    int p = net.addComponent(reg.create("Pipe"));
    int b = net.addComponent(reg.create("Boundary"));
    net.connect(a, "p", p, "in");
    net.connect(p, "out", b, "p");
    NodeGraph g = buildNodeGraph(net);
    CHECK(g.nodeCount == 2);
    CHECK(g.branches.size() == 1);
    CHECK(g.fixed[g.nodeOf(a, "p")]);
    CHECK(g.fixed[g.nodeOf(b, "p")]);
    CHECK(g.nodeOf(p, "in") == g.nodeOf(a, "p"));
  }

  // Boundary -> Pipe -> Junction -> Pipe -> Boundary : 3 nodes, mid free.
  {
    Network net;
    int a = net.addComponent(reg.create("Boundary"));
    int p1 = net.addComponent(reg.create("Pipe"));
    int j = net.addComponent(reg.create("Junction"));
    int p2 = net.addComponent(reg.create("Pipe"));
    int b = net.addComponent(reg.create("Boundary"));
    net.connect(a, "p", p1, "in");
    net.connect(p1, "out", j, "a");
    net.connect(j, "b", p2, "in");
    net.connect(p2, "out", b, "p");
    NodeGraph g = buildNodeGraph(net);
    CHECK(g.nodeCount == 3);
    CHECK(g.branches.size() == 2);
    int mid = g.nodeOf(p1, "out");
    CHECK(mid == g.nodeOf(p2, "in"));  // junction collapsed
    CHECK(!g.fixed[mid]);              // middle node free
  }

  return REPORT();
}

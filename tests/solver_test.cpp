#include <cmath>

#include "check.h"
#include "components/hydraulic/BranchLaw.h"
#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"
#include "core/Network.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();

  // Single pipe between two boundaries. All nodes fixed -> flow determined.
  Network net;
  auto bA = reg.create("Boundary");
  bA->params["pressure"] = 2.0e5;
  int a = net.addComponent(std::move(bA));
  int pipe = net.addComponent(reg.create("Pipe"));
  auto bB = reg.create("Boundary");
  bB->params["pressure"] = 1.0e5;
  int b = net.addComponent(std::move(bB));
  net.connect(a, "p", pipe, "in");
  net.connect(pipe, "out", b, "p");

  Results res;
  SolverManager mgr;
  SolveReport rep = mgr.runSteady(net, res);
  CHECK(rep.converged);

  // Self-consistency: the recorded flow matches the pipe's own law at the
  // converged pressure drop.
  NodeGraph g = buildNodeGraph(net);
  double Pin = res.latest("node." + std::to_string(g.nodeOf(pipe, "in")) + ".pressure");
  double Pout = res.latest("node." + std::to_string(g.nodeOf(pipe, "out")) + ".pressure");
  CHECK_NEAR(Pin, 2.0e5, 1e-3);
  CHECK_NEAR(Pout, 1.0e5, 1e-3);

  double Qrec = res.latest("comp." + std::to_string(pipe) + ".flow");
  FluidProps water = FluidLibrary::props("Light Water");
  BranchEval e = evalBranch(*net.component(pipe), Pin - Pout, water);
  CHECK(Qrec > 0);
  CHECK_NEAR(Qrec, e.Q, 1e-9);

  // A free-node case: Boundary -> Pipe -> (free node) -> Pipe -> Boundary.
  {
    Network n2;
    auto h = reg.create("Boundary");
    h->params["pressure"] = 3.0e5;
    int hi = n2.addComponent(std::move(h));
    int p1 = n2.addComponent(reg.create("Pipe"));
    int jn = n2.addComponent(reg.create("Junction"));
    int p2 = n2.addComponent(reg.create("Pipe"));
    auto l = reg.create("Boundary");
    l->params["pressure"] = 1.0e5;
    int lo = n2.addComponent(std::move(l));
    n2.connect(hi, "p", p1, "in");
    n2.connect(p1, "out", jn, "a");
    n2.connect(jn, "b", p2, "in");
    n2.connect(p2, "out", lo, "p");

    Results r2;
    SolveReport rep2 = mgr.runSteady(n2, r2);
    CHECK(rep2.converged);
    // Flow continuity: equal flow through both identical pipes.
    double q1 = r2.latest("comp." + std::to_string(p1) + ".flow");
    double q2 = r2.latest("comp." + std::to_string(p2) + ".flow");
    CHECK_NEAR(q1, q2, 1e-6);
    // Mid pressure between the two boundary pressures.
    NodeGraph g2 = buildNodeGraph(n2);
    double pmid = r2.latest("node." + std::to_string(g2.nodeOf(jn, "a")) + ".pressure");
    CHECK(pmid < 3.0e5 && pmid > 1.0e5);
    // Symmetric identical pipes -> mid ~ 2e5.
    CHECK_NEAR(pmid, 2.0e5, 5e3);
  }

  return REPORT();
}

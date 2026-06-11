// Exercises the multi-domain attachment ports, relief valves, the multi-tap
// air receiver, and integrated multi-mimic plant merging.
#include <cmath>

#include "check.h"
#include "components/hydraulic/BranchLaw.h"
#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  SolverManager mgr;

  // --- Attachment ports: Motor->Pump.drive and Actuator->Valve.act must NOT
  // create solver nodes, and the hydraulic network still solves. -----------
  {
    Network net;
    auto bA = reg.create("Boundary"); bA->params["pressure"] = 3.0e5;
    int a = net.addComponent(std::move(bA));
    int pump = net.addComponent(reg.create("Pump"));
    int valve = net.addComponent(reg.create("Valve"));
    auto bB = reg.create("Boundary"); bB->params["pressure"] = 1.0e5;
    int b = net.addComponent(std::move(bB));
    int motor = net.addComponent(reg.create("Motor"));
    int act = net.addComponent(reg.create("ElectricActuator"));
    net.connect(a, "p", pump, "in");
    net.connect(pump, "out", valve, "in");
    net.connect(valve, "out", b, "p");
    net.connect(motor, "t", pump, "drive");    // electrical drive link
    net.connect(act, "sig", valve, "act");      // actuator drive link

    NodeGraph g = buildNodeGraph(net);
    CHECK(g.nodeOf(pump, "drive") == -1);  // electrical port: no solver node
    CHECK(g.nodeOf(valve, "act") == -1);   // signal port: no solver node
    CHECK(g.branches.size() == 2);         // only pump + valve flow
    Results res;
    CHECK(mgr.runSteady(net, res).converged);
  }

  // --- Relief valve: shut below setpoint, open (passing flow) above it. -----
  {
    auto shutAndOpenFlow = [&](double pUp) {
      Network net;
      auto bA = reg.create("Boundary"); bA->params["pressure"] = pUp;
      int a = net.addComponent(std::move(bA));
      auto rv = reg.create("ReliefValve");
      rv->params["setpoint"] = 8.0e5;
      rv->params["blowdown"] = 5.0e4;
      int v = net.addComponent(std::move(rv));
      auto bB = reg.create("Boundary"); bB->params["pressure"] = 1.0e5;
      int b = net.addComponent(std::move(bB));
      net.connect(a, "p", v, "in");
      net.connect(v, "out", b, "p");
      Results res;
      mgr.runSteady(net, res);
      return res.latest("comp." + std::to_string(v) + ".flow");
    };
    // dP = 7e5-1e5 = 6e5 < 8e5 setpoint -> essentially shut.
    double qShut = shutAndOpenFlow(7.0e5);
    // dP = 2.0e6-1e5 = 1.9e6 > setpoint+blowdown -> fully open, real flow.
    double qOpen = shutAndOpenFlow(2.0e6);
    CHECK(std::fabs(qShut) < 1e-4);
    CHECK(qOpen > 1e-2);
    CHECK(qOpen > qShut * 100.0 + 1e-3);
  }

  // --- Air receiver: every tapping collapses onto one vessel node. ---------
  {
    Network net;
    auto bnd = reg.create("Boundary");
    bnd->fluid = "Air";
    bnd->params["pressure"] = 8.0e5;
    int b = net.addComponent(std::move(bnd));
    int duct = net.addComponent(reg.create("Duct"));
    auto arc = reg.create("AirReceiver");  // p_top 7e5, level 0 -> pins 7e5
    int r = net.addComponent(std::move(arc));
    net.connect(b, "p", duct, "in");
    net.connect(duct, "out", r, "n1");
    NodeGraph g = buildNodeGraph(net);
    CHECK(g.nodeOf(r, "p") == g.nodeOf(r, "n1"));   // tappings share one node
    CHECK(g.nodeOf(r, "p") == g.nodeOf(r, "n5"));
    Results res;
    CHECK(mgr.runSteady(net, res).converged);
    double q = res.latest("comp." + std::to_string(duct) + ".flow");
    CHECK(q > 0.0);  // 8e5 -> 7e5 receiver: charging flow in
  }

  // --- Integrated plant: two mimics share one receiver by linkTag. ---------
  {
    // Mimic A (supply): high boundary -> duct -> receiver(n1), tagged AR1.
    Network a;
    auto bs = reg.create("Boundary"); bs->fluid = "Air";
    bs->params["pressure"] = 8.5e5;
    int bsId = a.addComponent(std::move(bs));
    int dA = a.addComponent(reg.create("Duct"));
    auto ar1 = reg.create("AirReceiver");
    ar1->config["linkTag"] = "AR1";
    int ar1Id = a.addComponent(std::move(ar1));
    a.connect(bsId, "p", dA, "in");
    a.connect(dA, "out", ar1Id, "n1");

    // Mimic B (demand): same receiver(n2) -> duct -> low boundary.
    Network b;
    auto ar2 = reg.create("AirReceiver");
    ar2->config["linkTag"] = "AR1";  // SAME physical receiver
    int ar2Id = b.addComponent(std::move(ar2));
    int dB = b.addComponent(reg.create("Duct"));
    auto bd = reg.create("Boundary"); bd->fluid = "Air";
    bd->params["pressure"] = 1.0e5;
    int bdId = b.addComponent(std::move(bd));
    b.connect(ar2Id, "n2", dB, "in");
    b.connect(dB, "out", bdId, "p");

    std::vector<const Network*> mimics = {&a, &b};
    Network plant = mergeMimics(mimics);
    // One receiver dropped: 3 + 3 - 1 = 5 components, 2 + 2 = 4 connections.
    CHECK(plant.components().size() == 5);
    CHECK(plant.connections().size() == 4);
    // Exactly one AirReceiver survives.
    int receivers = 0;
    for (const auto& c : plant.components())
      if (c->type == "AirReceiver") ++receivers;
    CHECK(receivers == 1);

    Results res;
    CHECK(mgr.runSteady(plant, res).converged);  // integrated solve
  }

  return REPORT();
}

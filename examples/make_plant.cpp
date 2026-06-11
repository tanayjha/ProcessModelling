// Integrated multi-mimic plant example. Two subsystem mimics share one physical
// air receiver via a common linkTag ("AR-PLANT-1"):
//   * compressor_house.umpnap : a motor-driven compressor charges the receiver,
//     which is protected by an air safety relief valve.
//   * instrument_air.umpnap   : a pneumatically-actuated valve draws air from
//     the SAME receiver to a consumer.
// A plant manifest (plant.umpproj) lists both. Loading the manifest merges them
// into one network — the shared receiver becomes a single tie-point node — so
// the whole plant solves in unison (compressor supply balances valve demand).
#include <cstdio>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"
#include "solver/SolverManager.h"

using namespace umpnap;

namespace {
// Receiver pressure kept just above atmospheric so the gas head curves (modeled
// in metres, as for the hydraulic machines) can charge and discharge it.
constexpr double kRecvTop = 1.03e5;
constexpr double kAtm = 1.013e5;

std::unique_ptr<Component> recv(const char* tag) {
  auto& reg = ComponentRegistry::instance();
  auto r = reg.create("AirReceiver");
  r->name = tag;
  r->config["linkTag"] = "AR-PLANT-1";  // SAME physical receiver in both mimics
  r->params["p_top"] = kRecvTop;
  r->params["level"] = 0.0;
  return r;
}
}  // namespace

int main(int argc, char** argv) {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  std::string dir = (argc > 1) ? argv[1] : "examples/";

  // ---- Mimic A: compressor house ----------------------------------------
  Network a;
  auto atm = reg.create("Boundary");
  atm->fluid = "Air"; atm->params["pressure"] = kAtm;
  atm->x = 60; atm->y = 80;
  int iAtm = a.addComponent(std::move(atm));

  auto comp = reg.create("Compressor");  // defaults to Air
  comp->name = "K-101"; comp->x = 250; comp->y = 80;
  int iK = a.addComponent(std::move(comp));

  auto motor = reg.create("Motor");
  motor->name = "M-101"; motor->x = 250; motor->y = 260;
  int iM = a.addComponent(std::move(motor));

  auto arA = recv("AR-101"); arA->x = 480; arA->y = 80;
  int iAR = a.addComponent(std::move(arA));

  auto psv = reg.create("GasReliefValve");
  psv->name = "PSV-101"; psv->params["setpoint"] = 5.0e5;  // shut in normal op
  psv->x = 480; psv->y = 320;
  int iPSV = a.addComponent(std::move(psv));

  auto vent = reg.create("Boundary");
  vent->fluid = "Air"; vent->params["pressure"] = kAtm;
  vent->x = 700; vent->y = 320;
  int iVent = a.addComponent(std::move(vent));

  a.connect(iAtm, "p", iK, "in");
  a.connect(iK, "out", iAR, "n1");
  a.connect(iM, "t", iK, "drive");      // motor drives the compressor
  a.connect(iAR, "n2", iPSV, "in");
  a.connect(iPSV, "out", iVent, "p");
  saveProject(a, dir + "compressor_house.umpnap");

  // ---- Mimic B: instrument-air distribution -----------------------------
  Network b;
  auto arB = recv("AR-201"); arB->x = 60; arB->y = 80;  // same receiver (linkTag)
  int jAR = b.addComponent(std::move(arB));

  auto valve = reg.create("Valve");
  valve->name = "V-201"; valve->fluid = "Air";
  valve->params["position"] = 0.6;
  valve->x = 280; valve->y = 80;
  int jV = b.addComponent(std::move(valve));

  auto pa = reg.create("PneumaticActuatorModulating");
  pa->name = "PA-201"; pa->x = 280; pa->y = 260;
  int jPA = b.addComponent(std::move(pa));

  auto user = reg.create("Boundary");
  user->fluid = "Air"; user->params["pressure"] = kAtm;
  user->x = 500; user->y = 80;
  int jU = b.addComponent(std::move(user));

  b.connect(jAR, "n3", jV, "in");
  b.connect(jV, "out", jU, "p");
  b.connect(jPA, "sig", jV, "act");     // pneumatic actuator drives the valve
  saveProject(b, dir + "instrument_air.umpnap");

  // ---- Plant manifest ----------------------------------------------------
  PlantProject proj;
  proj.name = "Demo Air Plant";
  proj.mimics = {"compressor_house.umpnap", "instrument_air.umpnap"};
  std::string projPath = dir + "plant.umpproj";
  savePlant(proj, projPath);
  std::printf("Wrote %scompressor_house.umpnap, %sinstrument_air.umpnap, %s\n",
              dir.c_str(), dir.c_str(), projPath.c_str());

  // ---- Load merged plant and solve in unison -----------------------------
  Network plant;
  if (!loadPlantNetwork(plant, projPath)) {
    std::fprintf(stderr, "failed to load plant\n");
    return 1;
  }
  int receivers = 0;
  for (const auto& c : plant.components())
    if (c->type == "AirReceiver") ++receivers;
  std::printf("Merged plant: %zu components, %zu connections, %d receiver(s)\n",
              plant.components().size(), plant.connections().size(), receivers);

  Results res;
  SolverManager mgr;
  SolveReport rep = mgr.runSteady(plant, res);
  std::printf("Integrated steady: %s (iters=%d)\n", rep.message.c_str(),
              rep.iterations);

  NodeGraph g = buildNodeGraph(plant);
  for (const auto& c : plant.components()) {
    if (c->type == "AirReceiver") {
      int n = g.nodeOf(c->id, "p");
      std::printf("Receiver pressure = %.0f Pa\n",
                  res.latest("node." + std::to_string(n) + ".pressure"));
    }
    if (c->type == "Compressor" || c->type == "Valve")
      std::printf("%s flow = %.4f m^3/s\n", c->name.c_str(),
                  res.latest("comp." + std::to_string(c->id) + ".flow"));
  }
  return rep.converged ? 0 : 2;
}

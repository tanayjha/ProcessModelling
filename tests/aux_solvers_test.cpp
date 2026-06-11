// Exercises the steam pressure-flow solver, the electrical DC power-flow solver,
// and the split shell-and-tube thermal coupling.
#include <cmath>
#include <string>

#include "check.h"
#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Results.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main() {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  SolverManager mgr;

  // --- Electrical: Grid -> Cable -> Motor. Voltage drops, motor draws power. -
  {
    Network net;
    auto grid = reg.create("Grid"); grid->params["voltage"] = 415.0;
    int g = net.addComponent(std::move(grid));
    int cab = net.addComponent(reg.create("Cable"));
    int mot = net.addComponent(reg.create("Motor"));
    net.connect(g, "t", cab, "in");
    net.connect(cab, "out", mot, "t");
    Results res;
    mgr.runSteady(net, res);
    double vsrc = res.latest("comp." + std::to_string(g) + ".voltage");
    double vmot = res.latest("comp." + std::to_string(mot) + ".voltage");
    double pmot = res.latest("comp." + std::to_string(mot) + ".power");
    double isrc = res.latest("comp." + std::to_string(g) + ".current");
    CHECK_NEAR(vsrc, 415.0, 1e-6);
    CHECK(vmot < vsrc);          // cable IR drop
    CHECK(vmot > 380.0);         // small drop for a short cable
    CHECK(pmot > 1.0e3);         // motor consumes power
    CHECK(isrc > 0.0);           // source delivers current
  }

  // --- Transformer: ideal turns-ratio steps 11 kV down to ~415 V. -----------
  {
    Network net;
    auto grid = reg.create("Grid"); grid->params["voltage"] = 11000.0;
    int g = net.addComponent(std::move(grid));
    auto tx = reg.create("Transformer"); tx->params["ratio"] = 26.5;
    int x = net.addComponent(std::move(tx));
    auto ld = reg.create("ElectricalLoad"); ld->params["power"] = 100.0;
    int l = net.addComponent(std::move(ld));
    net.connect(g, "t", x, "hv");
    net.connect(x, "lv", l, "t");
    Results res;
    mgr.runSteady(net, res);
    double vlv = res.latest("comp." + std::to_string(l) + ".voltage");
    CHECK_NEAR(vlv, 11000.0 / 26.5, 1.0);  // ideal ratio
    CHECK(res.latest("comp." + std::to_string(l) + ".power") > 0.0);
  }

  // --- Steam: SteamGenerator -> Turbine -> Condenser. Flow + power. ----------
  {
    Network net;
    int sg = net.addComponent(reg.create("SteamGenerator"));  // 4.7 MPa
    int tu = net.addComponent(reg.create("Turbine"));
    int cn = net.addComponent(reg.create("Condenser"));       // 5 kPa
    net.connect(sg, "steam", tu, "steam");
    net.connect(tu, "exhaust", cn, "steam");
    Results res;
    mgr.runSteady(net, res);
    double q = res.latest("comp." + std::to_string(tu) + ".flow");
    double pw = res.latest("comp." + std::to_string(tu) + ".power");
    CHECK(q > 100.0);     // hundreds of kg/s of steam
    CHECK(pw > 1.0e8);    // hundreds of MW
  }

  // --- Split HX: ShellSide (cold) + TubeSide (hot) share thermalTag. --------
  {
    Network net;
    // Hot tube loop (heavy water, 90 C in).
    auto ha = reg.create("Boundary"); ha->params["pressure"] = 3.0e5;
    ha->fluid = "Heavy Water (D2O)";
    int hA = net.addComponent(std::move(ha));
    auto tube = reg.create("TubeSide");
    tube->fluid = "Heavy Water (D2O)";
    tube->params["Tin"] = 90.0;
    tube->config["thermalTag"] = "HX1";
    int tb = net.addComponent(std::move(tube));
    auto hb = reg.create("Boundary"); hb->params["pressure"] = 1.0e5;
    hb->fluid = "Heavy Water (D2O)";
    int hB = net.addComponent(std::move(hb));
    net.connect(hA, "p", tb, "in");
    net.connect(tb, "out", hB, "p");
    // Cold shell loop (light water, 30 C in).
    auto ca = reg.create("Boundary"); ca->params["pressure"] = 3.0e5;
    int cA = net.addComponent(std::move(ca));
    auto shell = reg.create("ShellSide");
    shell->params["Tin"] = 30.0;
    shell->config["thermalTag"] = "HX1";
    int sh = net.addComponent(std::move(shell));
    auto cb = reg.create("Boundary"); cb->params["pressure"] = 1.0e5;
    int cB = net.addComponent(std::move(cb));
    net.connect(cA, "p", sh, "in");
    net.connect(sh, "out", cB, "p");

    Results res;
    mgr.runSteady(net, res);
    double qt = std::fabs(res.latest("comp." + std::to_string(tb) + ".flow"));
    double qs = std::fabs(res.latest("comp." + std::to_string(sh) + ".flow"));
    CHECK(qt > 0.0);
    CHECK(qs > 0.0);
    double duty = res.latest("comp." + std::to_string(tb) + ".duty");
    double ttout = res.latest("comp." + std::to_string(tb) + ".Tout");
    double tsout = res.latest("comp." + std::to_string(sh) + ".Tout");
    CHECK(duty > 0.0);
    CHECK(ttout < 90.0);   // hot stream cooled
    CHECK(ttout > 30.0);
    CHECK(tsout > 30.0);   // cold stream warmed
    CHECK(tsout < 90.0);
  }

  return REPORT();
}

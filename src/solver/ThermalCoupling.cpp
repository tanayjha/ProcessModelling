#include "solver/ThermalCoupling.h"

#include <cmath>
#include <map>
#include <string>

#include "core/FluidLibrary.h"
#include "core/Results.h"

namespace umpnap {

namespace {

// Counterflow effectiveness for capacity ratio Cr = Cmin/Cmax and NTU.
//   Cr < 1:  eps = (1 - e^{-NTU(1-Cr)}) / (1 - Cr e^{-NTU(1-Cr)})
//   Cr = 1:  eps = NTU / (1 + NTU)
double effectivenessCounterflow(double ntu, double cr) {
  if (ntu <= 0.0) return 0.0;
  if (cr >= 0.999) return ntu / (1.0 + ntu);
  double e = std::exp(-ntu * (1.0 - cr));
  return (1.0 - e) / (1.0 - cr * e);
}

}  // namespace

void applyThermalCoupling(const Network& net, Results& out, double t) {
  // Index every side by thermalTag so a ShellSide can find its TubeSide.
  std::map<std::string, const Component*> shellByTag, tubeByTag;
  for (const auto& c : net.components()) {
    std::string tag = c->cfg("thermalTag");
    if (tag.empty()) continue;
    if (c->type == "ShellSide") shellByTag[tag] = c.get();
    else if (c->type == "TubeSide") tubeByTag[tag] = c.get();
  }

  for (const auto& kv : tubeByTag) {
    auto sit = shellByTag.find(kv.first);
    if (sit == shellByTag.end()) continue;  // no shell partner for this tube
    const Component* tube = kv.second;
    const Component* shell = sit->second;

    // Per-stream capacity rate  C = rho * |Q| * cp  [W/K].
    auto capacity = [&](const Component* c) {
      FluidProps f = FluidLibrary::props(c->fluid);
      double Q = std::fabs(out.latest("comp." + std::to_string(c->id) + ".flow"));
      return f.density * Q * f.cp;
    };
    double Ct = capacity(tube);
    double Cs = capacity(shell);
    double Tt = tube->param("Tin");
    double Ts = shell->param("Tin");

    // Record inlet temps regardless; default outlets to inlets (no transfer).
    double TtOut = Tt, TsOut = Ts, duty = 0.0;

    if (Ct > 1e-9 && Cs > 1e-9) {
      double Cmin = std::min(Ct, Cs);
      double Cmax = std::max(Ct, Cs);
      double Cr = Cmin / Cmax;
      // UA from the tube side: A_s = N * pi * d_o * L (outer tube surface).
      double N = tube->param("numTubes");
      double doo = tube->param("tubeOD");
      double L = tube->param("tubeLength");
      double U = tube->param("U");
      double UA = U * N * 3.14159265358979323846 * doo * L;
      double ntu = (Cmin > 0.0) ? UA / Cmin : 0.0;
      double eps = effectivenessCounterflow(ntu, Cr);
      duty = eps * Cmin * std::fabs(Tt - Ts);  // [W], hot -> cold
      // Hot stream cools, cold stream warms.
      double dTt = duty / Ct;
      double dTs = duty / Cs;
      if (Tt >= Ts) { TtOut = Tt - dTt; TsOut = Ts + dTs; }
      else          { TtOut = Tt + dTt; TsOut = Ts - dTs; }
    }

    auto rec = [&](const Component* c, double Tin, double Tout) {
      std::string p = "comp." + std::to_string(c->id) + ".";
      out.record(p + "duty", t, duty);
      out.record(p + "Tin", t, Tin);
      out.record(p + "Tout", t, Tout);
    };
    rec(tube, Tt, TtOut);
    rec(shell, Ts, TsOut);
  }
}

}  // namespace umpnap

// Loads drain.umpnap, runs a transient, feeds the recorded series into a real
// TrendWidget, and renders it to PNG so we can see what the GUI would draw.
#include <QApplication>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include "core/Results.h"
#include "gui/TrendDock.h"
#include "gui/TrendWidget.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  registerHydraulicComponents();

  Network net;
  if (!loadProject(net, argc > 1 ? argv[1] : "examples/drain.umpnap")) return 1;

  Results res;
  SolverManager mgr;
  mgr.runTransient(net, res, 1.0, 30);

  // Print every key so we know exactly what appears in the Trends list.
  for (const auto& k : res.keys()) {
    const auto* s = res.series(k);
    double first = s->front().second, last = s->back().second;
    qInfo("key=%-22s  n=%zu  first=%.4g  last=%.4g", k.c_str(), s->size(), first,
          last);
  }

  // Render the REAL TrendDock the way the GUI uses it: refreshKeys() now
  // auto-selects the varying signals, so a curve should appear with no clicks.
  TrendDock dock(&res);
  dock.resize(680, 360);
  dock.refreshKeys();

  QPixmap pm = dock.grab();
  const char* out = argc > 2 ? argv[2] : "docs/umpnap_trend.png";
  pm.save(out);
  qInfo("wrote %s", out);
  return 0;
}

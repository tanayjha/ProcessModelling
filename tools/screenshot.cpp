// Offscreen render of the real DiagramScene loaded from a project file, to a
// PNG. Used to visually verify the canvas renders components and wires.
#include <QApplication>
#include <QImage>
#include <QPainter>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include "core/Results.h"
#include "gui/DiagramScene.h"
#include "solver/SolverManager.h"

using namespace umpnap;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  registerHydraulicComponents();

  Network net;
  const char* in = (argc > 1) ? argv[1] : "examples/loop.umpnap";
  const char* out = (argc > 2) ? argv[2] : "docs/umpnap_canvas.png";
  int steps = (argc > 3) ? atoi(argv[3]) : 0;  // run sim steps for runtime overlay
  if (!loadProject(net, in)) {
    qWarning("failed to load %s", in);
    return 1;
  }

  DiagramScene scene(&net);
  scene.rebuildFromNetwork();

  // Optionally run the simulation and push live values onto the diagram.
  Results res;
  if (steps > 0) {
    SolverManager mgr;
    mgr.runSteady(net, res);
    for (int s = 1; s <= steps; ++s) mgr.stepTransient(net, res, 1.0, s);
    scene.updateRuntime(res);
  }

  QRectF r = scene.itemsBoundingRect().adjusted(-30, -30, 40, 40);

  QImage img((int)r.width(), (int)r.height(), QImage::Format_ARGB32);
  img.fill(Qt::white);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  scene.render(&p, QRectF(), r);
  p.end();

  if (!img.save(out)) {
    qWarning("failed to save %s", out);
    return 1;
  }
  qInfo("wrote %s (%dx%d)", out, img.width(), img.height());
  return 0;
}

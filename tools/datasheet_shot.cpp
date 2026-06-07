// Offscreen render of the Plant Data datasheet for a project, to verify the
// tabbed tables populate with each component's design fields.
#include <QApplication>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
#include <QTabWidget>
#include <cstdlib>

#include "gui/PlantData.h"

using namespace umpnap;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  registerHydraulicComponents();
  Network net;
  if (!loadProject(net, argc > 1 ? argv[1] : "examples/loop.umpnap")) return 1;

  PlantDataDialog dlg(&net);
  dlg.resize(900, 360);
  dlg.show();
  if (auto* tw = dlg.findChild<QTabWidget*>()) tw->setCurrentIndex(argc>3?atoi(argv[3]):0);
  app.processEvents();
  QPixmap pm = dlg.grab();
  pm.save(argc > 2 ? argv[2] : "docs/umpnap_plantdata.png");
  return 0;
}

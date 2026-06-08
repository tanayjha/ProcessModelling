#include <QApplication>
#include "core/ComponentRegistry.h"
#include "gui/PaletteDock.h"
using namespace umpnap;
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  registerHydraulicComponents();
  PaletteDock dock;
  dock.resize(240, 640);
  dock.show();
  app.processEvents();
  dock.grab().save(argc > 1 ? argv[1] : "docs/umpnap_palette.png");
  return 0;
}

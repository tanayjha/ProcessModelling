// Offscreen render of the real MainWindow (tabs, docks, project tree) after
// opening a mimic or a plant project. Verifies the multi-document UI renders.
// Usage: umpnap_window <project.umpnap|plant.umpproj> <out.png> [tabIndex]
#include <QApplication>
#include <QTabWidget>
#include <QTimer>

#include "gui/MainWindow.h"

using namespace umpnap;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  MainWindow w;
  w.resize(1400, 900);
  w.show();

  QString in = argc > 1 ? argv[1] : "examples/plant.umpproj";
  QString out = argc > 2 ? argv[2] : "docs/umpnap_tabs.png";
  int tab = argc > 3 ? atoi(argv[3]) : -1;
  if (in.endsWith(".umpproj"))
    w.openPlantPath(in);
  else
    w.openPath(in);

  // Let layout settle, optionally select a tab, then grab to PNG and quit.
  QTimer::singleShot(300, [&]() {
    if (tab >= 0) {
      if (auto* tabs = w.findChild<QTabWidget*>()) tabs->setCurrentIndex(tab);
    }
    QTimer::singleShot(150, [&]() {
      w.grab().save(out);
      app.quit();
    });
  });
  return app.exec();
}

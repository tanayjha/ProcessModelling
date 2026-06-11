#include <QApplication>
#include <QString>

#include "gui/MainWindow.h"

// Usage: umpnap [project.umpnap | plant.umpproj] [--run]
//   Optionally opens a single mimic (.umpnap) or an integrated plant project
//   (.umpproj, all mimics merged) on launch and starts the simulation.
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  umpnap::MainWindow w;
  w.show();

  QString openFile;
  bool autoRun = false;
  for (int i = 1; i < argc; ++i) {
    QString a = argv[i];
    if (a == "--run")
      autoRun = true;
    else if (!a.startsWith("-"))
      openFile = a;
  }
  if (!openFile.isEmpty()) {
    if (openFile.endsWith(".umpproj"))
      w.openPlantPath(openFile);
    else
      w.openPath(openFile);
  }
  if (autoRun) w.startSimulation();

  return app.exec();
}

#include <QApplication>

#include "gui/MainWindow.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  umpnap::MainWindow w;
  w.show();
  return app.exec();
}

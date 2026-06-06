#include "gui/MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QGraphicsView>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QStatusBar>
#include <QToolBar>

#include "core/ComponentRegistry.h"
#include "core/Project.h"
#include "gui/DiagramScene.h"
#include "gui/HierarchyDock.h"
#include "gui/PaletteDock.h"
#include "gui/PropertyEditor.h"
#include "gui/TrendDock.h"
#include "solver/Validation.h"

namespace umpnap {

MainWindow::MainWindow() {
  registerHydraulicComponents();
  setWindowTitle("UMPNAP - Unified Multi-Domain Process Network Analysis Platform");
  resize(1280, 840);

  scene_ = new DiagramScene(&net_, this);
  view_ = new QGraphicsView(scene_, this);
  view_->setRenderHint(QPainter::Antialiasing, true);
  view_->setDragMode(QGraphicsView::NoDrag);
  setCentralWidget(view_);

  palette_ = new PaletteDock(this);
  properties_ = new PropertyEditor(this);
  hierarchy_ = new HierarchyDock(this);
  trends_ = new TrendDock(&results_, this);

  addDockWidget(Qt::LeftDockWidgetArea, palette_);
  addDockWidget(Qt::LeftDockWidgetArea, hierarchy_);
  addDockWidget(Qt::RightDockWidgetArea, properties_);
  addDockWidget(Qt::BottomDockWidgetArea, trends_);

  connect(palette_, &PaletteDock::typeSelected, scene_, &DiagramScene::setArmedType);
  connect(scene_, &DiagramScene::componentSelected, properties_,
          &PropertyEditor::showComponent);
  connect(scene_, &DiagramScene::networkChanged, this,
          [this]() { hierarchy_->refresh(&net_); });
  connect(properties_, &PropertyEditor::edited, this, [this]() {
    scene_->update();
    hierarchy_->refresh(&net_);
  });
  connect(hierarchy_, &HierarchyDock::componentActivated, scene_,
          &DiagramScene::selectComponent);

  buildMenus();
  statusBar()->showMessage(
      "Ready. Click a palette item, then click the canvas to place it. "
      "Drag port-to-port to connect.");
}

void MainWindow::buildMenus() {
  QMenu* file = menuBar()->addMenu("&File");
  file->addAction("&New", this, &MainWindow::newProject);
  file->addAction("&Open...", this, &MainWindow::openProject);
  file->addAction("&Save...", this, &MainWindow::saveProject_);
  file->addSeparator();
  file->addAction("&Export Results CSV...", this, &MainWindow::exportCsv);
  file->addSeparator();
  file->addAction("&Quit", this, &QWidget::close);

  QMenu* run = menuBar()->addMenu("&Run");
  run->addAction("Run &Steady", this, &MainWindow::runSteady);
  run->addAction("Run &Transient...", this, &MainWindow::runTransient);

  QMenu* help = menuBar()->addMenu("&Help");
  help->addAction("&Validate Solver", this, &MainWindow::runValidation);
  help->addAction("&About", this, &MainWindow::about);

  QToolBar* tb = addToolBar("Main");
  tb->addAction("Steady", this, &MainWindow::runSteady);
  tb->addAction("Transient", this, &MainWindow::runTransient);
  tb->addAction("Validate", this, &MainWindow::runValidation);
}

void MainWindow::newProject() {
  net_.clear();
  results_.clear();
  scene_->rebuildFromNetwork();
  hierarchy_->refresh(&net_);
  trends_->refreshKeys();
  properties_->showComponent(nullptr);
  statusBar()->showMessage("New project.");
}

void MainWindow::openProject() {
  QString path = QFileDialog::getOpenFileName(this, "Open Project", QString(),
                                              "UMPNAP Projects (*.umpnap)");
  if (path.isEmpty()) return;
  if (!loadProject(net_, path.toStdString())) {
    QMessageBox::warning(this, "Open", "Failed to load project.");
    return;
  }
  scene_->rebuildFromNetwork();
  hierarchy_->refresh(&net_);
  properties_->showComponent(nullptr);
  statusBar()->showMessage("Opened " + path);
}

void MainWindow::saveProject_() {
  QString path = QFileDialog::getSaveFileName(this, "Save Project", QString(),
                                              "UMPNAP Projects (*.umpnap)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".umpnap")) path += ".umpnap";
  if (!saveProject(net_, path.toStdString())) {
    QMessageBox::warning(this, "Save", "Failed to save project.");
    return;
  }
  statusBar()->showMessage("Saved " + path);
}

void MainWindow::exportCsv() {
  QString path = QFileDialog::getSaveFileName(this, "Export Results", QString(),
                                              "CSV (*.csv)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".csv")) path += ".csv";
  if (!results_.writeCsv(path.toStdString())) {
    QMessageBox::warning(this, "Export", "Failed to write CSV.");
    return;
  }
  statusBar()->showMessage("Exported results to " + path);
}

void MainWindow::runSteady() {
  std::string err = net_.validate();
  if (!err.empty()) {
    QMessageBox::warning(this, "Cannot solve", QString::fromStdString(err));
    return;
  }
  SolveReport rep = solver_.runSteady(net_, results_);
  trends_->refreshKeys();
  QString msg = QString("Steady solve: %1  (iters=%2, residual=%3)")
                    .arg(QString::fromStdString(rep.message))
                    .arg(rep.iterations)
                    .arg(rep.residual, 0, 'e', 2);
  statusBar()->showMessage(msg);
  if (!rep.converged)
    QMessageBox::warning(this, "Alarm: solve did not converge", msg);
}

void MainWindow::runTransient() {
  std::string err = net_.validate();
  if (!err.empty()) {
    QMessageBox::warning(this, "Cannot solve", QString::fromStdString(err));
    return;
  }
  bool ok = false;
  double dt = QInputDialog::getDouble(this, "Transient", "Timestep dt (s):", 1.0,
                                      1e-6, 1e6, 4, &ok);
  if (!ok) return;
  int steps = QInputDialog::getInt(this, "Transient", "Number of steps:", 30, 1,
                                   100000, 1, &ok);
  if (!ok) return;
  SolveReport rep = solver_.runTransient(net_, results_, dt, steps);
  scene_->update();
  hierarchy_->refresh(&net_);
  trends_->refreshKeys();
  statusBar()->showMessage(
      QString("Transient: %1 steps of dt=%2 s. Final: %3")
          .arg(steps)
          .arg(dt)
          .arg(QString::fromStdString(rep.message)));
}

void MainWindow::runValidation() {
  ValidationResult vr = runPipeNetworkValidation(1e-3);
  QString body = QString("%1\n\nMax relative error: %2\n\n%3")
                     .arg(vr.passed ? "PASSED" : "FAILED")
                     .arg(vr.maxRelError, 0, 'e', 3)
                     .arg(QString::fromStdString(vr.detail));
  if (vr.passed)
    QMessageBox::information(this, "Solver Validation", body);
  else
    QMessageBox::warning(this, "Solver Validation", body);
}

void MainWindow::about() {
  QMessageBox::about(
      this, "About UMPNAP",
      "Unified Multi-Domain Process Network Analysis Platform\n\n"
      "Phase 1: single-phase hydraulic network solver (Newton-Raphson, "
      "nodal pressure formulation).\n\n"
      "Generic fluid property package, plugin component library, and an "
      "analytically-validated solver. Gas/thermal/electrical domains are "
      "registered as solver stubs for later phases.");
}

}  // namespace umpnap

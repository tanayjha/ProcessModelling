#include "gui/MainWindow.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QStatusBar>
#include <QToolBar>
#include <memory>

#include "core/ComponentRegistry.h"
#include "core/Project.h"
#include "gui/DiagramScene.h"
#include "gui/HierarchyDock.h"
#include "gui/PaletteDock.h"
#include "gui/PlantData.h"
#include "gui/PropertyEditor.h"
#include "gui/SimController.h"
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
  // Rubber-band selection over empty canvas; the scene intercepts presses on
  // ports (wiring) and palette placement, so this only drags a selection box.
  view_->setDragMode(QGraphicsView::RubberBandDrag);
  setCentralWidget(view_);

  palette_ = new PaletteDock(this);
  properties_ = new PropertyEditor(this);
  hierarchy_ = new HierarchyDock(this);
  trends_ = new TrendDock(&results_, this);

  addDockWidget(Qt::LeftDockWidgetArea, palette_);
  addDockWidget(Qt::LeftDockWidgetArea, hierarchy_);
  addDockWidget(Qt::RightDockWidgetArea, properties_);
  addDockWidget(Qt::BottomDockWidgetArea, trends_);
  resizeDocks({trends_}, {300}, Qt::Vertical);

  sim_ = new SimController(&net_, &results_, this);
  connect(sim_, &SimController::updated, this, &MainWindow::onSimUpdated);
  connect(sim_, &SimController::modeChanged, this, &MainWindow::onSimModeChanged);

  connect(palette_, &PaletteDock::typeSelected, scene_, &DiagramScene::setArmedType);
  connect(scene_, &DiagramScene::componentSelected, this, [this](Component* c) {
    selected_ = c;
    properties_->showComponent(c);
  });
  connect(scene_, &DiagramScene::networkChanged, this,
          [this]() { hierarchy_->refresh(&net_); });
  connect(scene_, &DiagramScene::connectionRejected, this,
          [this](const QString& r) { statusBar()->showMessage(r, 5000); });
  connect(properties_, &PropertyEditor::edited, this, [this]() {
    scene_->update();
    hierarchy_->refresh(&net_);
  });
  connect(hierarchy_, &HierarchyDock::componentActivated, scene_,
          &DiagramScene::selectComponent);

  buildMenus();
  buildSimToolbar();
  statusBar()->showMessage(
      "Ready. Click a palette item, then click the canvas to place it. "
      "Drag port-to-port to connect. Use the Simulation toolbar to run.");
}

void MainWindow::buildMenus() {
  QMenu* file = menuBar()->addMenu("&File");
  file->addAction("&New", this, &MainWindow::newProject);
  file->addAction("&Open...", this, &MainWindow::openProject);
  file->addAction("&Save...", this, &MainWindow::saveProject_);
  file->addSeparator();
  file->addAction("Save &Initial Condition...", this,
                  &MainWindow::saveInitialCondition);
  file->addAction("Load Initial &Condition...", this,
                  &MainWindow::loadInitialCondition);
  file->addSeparator();
  file->addAction("&Export Results CSV...", this, &MainWindow::exportCsv);
  file->addSeparator();
  file->addAction("&Quit", this, &QWidget::close);

  QMenu* edit = menuBar()->addMenu("&Edit");
  edit->addAction("&Plant Data...", this, &MainWindow::openPlantData);
  edit->addAction("&Clone Selected", QKeySequence("Ctrl+D"), this,
                  &MainWindow::cloneSelected);

  QMenu* sim = menuBar()->addMenu("&Simulation");
  sim->addAction("&Initialize (Steady)", this,
                 [this]() { sim_->initializeSteady(); });
  sim->addAction("&Run", this, [this]() { sim_->start(); });
  sim->addAction("&Pause / Freeze", this, [this]() { sim_->pause(); });
  sim->addAction("Single &Step", this, [this]() { sim_->singleStep(); });
  sim->addAction("Rese&t", this, [this]() { sim_->reset(); });
  sim->addSeparator();
  sim->addAction("Save Snapshot", this, [this]() { sim_->saveSnapshot(); });
  sim->addAction("Restore Snapshot", this, [this]() { sim_->restoreSnapshot(); });
  sim->addAction("Set &Timestep...", this, [this]() {
    bool ok = false;
    double dt = QInputDialog::getDouble(this, "Timestep", "dt (s):", sim_->dt(),
                                        1e-6, 1e6, 4, &ok);
    if (ok) sim_->setDt(dt);
  });

  // View menu: show/hide and restore the docks.
  QMenu* view = menuBar()->addMenu("&View");
  for (QDockWidget* d : {static_cast<QDockWidget*>(palette_),
                         static_cast<QDockWidget*>(hierarchy_),
                         static_cast<QDockWidget*>(properties_),
                         static_cast<QDockWidget*>(trends_)})
    view->addAction(d->toggleViewAction());
  view->addSeparator();
  view->addAction("Restore All Panels", this, [this]() {
    for (QDockWidget* d : {static_cast<QDockWidget*>(palette_),
                           static_cast<QDockWidget*>(hierarchy_),
                           static_cast<QDockWidget*>(properties_),
                           static_cast<QDockWidget*>(trends_)}) {
      d->show();
      d->setFloating(false);
    }
    addDockWidget(Qt::LeftDockWidgetArea, palette_);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchy_);
    addDockWidget(Qt::RightDockWidgetArea, properties_);
    addDockWidget(Qt::BottomDockWidgetArea, trends_);
    resizeDocks({trends_}, {300}, Qt::Vertical);
  });

  QMenu* help = menuBar()->addMenu("&Help");
  help->addAction("&Validate Solver", this, &MainWindow::runValidation);
  help->addAction("&About", this, &MainWindow::about);
}

void MainWindow::buildSimToolbar() {
  QToolBar* tb = addToolBar("Simulation");
  tb->addAction("Plant Data", this, &MainWindow::openPlantData);
  tb->addSeparator();
  tb->addAction("⏮ Init", this, [this]() { sim_->initializeSteady(); });
  tb->addAction("▶ Run", this, [this]() { sim_->start(); });
  tb->addAction("⏸ Pause", this, [this]() { sim_->pause(); });
  tb->addAction("⏭ Step", this, [this]() { sim_->singleStep(); });
  tb->addAction("⟲ Reset", this, [this]() { sim_->reset(); });
  tb->addSeparator();
  tb->addWidget(new QLabel(" Speed ", tb));
  auto* speed = new QComboBox(tb);
  speed->addItems({"1x", "2x", "5x", "10x", "20x"});
  connect(speed, &QComboBox::currentTextChanged, this, [this](const QString& s) {
    sim_->setSpeed(s.left(s.size() - 1).toInt());
  });
  tb->addWidget(speed);
  tb->addSeparator();
  tb->addAction("Snapshot", this, [this]() { sim_->saveSnapshot(); });
  tb->addAction("Restore", this, [this]() { sim_->restoreSnapshot(); });
  tb->addAction("Validate", this, &MainWindow::runValidation);
  tb->addSeparator();
  tb->addWidget(new QLabel(" Find tag ", tb));
  auto* search = new QLineEdit(tb);
  search->setPlaceholderText("e.g. P-2");
  search->setMaximumWidth(120);
  search->setClearButtonEnabled(true);
  connect(search, &QLineEdit::returnPressed, this, [this, search]() {
    QString q = search->text().trimmed();
    if (q.isEmpty()) return;
    Component* c = net_.componentByName(q.toStdString());
    if (!c) {  // fall back to a case-insensitive partial tag match
      for (const auto& comp : net_.components())
        if (QString::fromStdString(comp->name).contains(q, Qt::CaseInsensitive)) {
          c = comp.get();
          break;
        }
    }
    if (c) {
      scene_->selectComponent(c->id);  // -> selects + shows in Properties
      view_->centerOn(c->x, c->y);
      statusBar()->showMessage("Found " + QString::fromStdString(c->name));
    } else {
      statusBar()->showMessage("No component matching '" + q + "'");
    }
  });
  tb->addWidget(search);
  tb->addSeparator();
  clock_ = new QLabel("  t = 0.0 s   [Stopped]  ", tb);
  tb->addWidget(clock_);
}

void MainWindow::onSimUpdated() {
  scene_->updateRuntime(results_);
  trends_->liveUpdate();
  if (clock_) {
    const char* m = sim_->mode() == SimController::Running ? "Running"
                    : sim_->mode() == SimController::Paused ? "Paused"
                                                            : "Stopped";
    clock_->setText(QString("  t = %1 s   [%2]  ")
                        .arg(sim_->time(), 0, 'f', 1)
                        .arg(m));
  }
}

void MainWindow::onSimModeChanged() {
  onSimUpdated();
  const char* m = sim_->mode() == SimController::Running ? "Running"
                  : sim_->mode() == SimController::Paused ? "Paused (inspect/edit allowed)"
                                                          : "Stopped";
  statusBar()->showMessage(QString("Simulation: %1").arg(m));
}

void MainWindow::cloneSelected() {
  if (!selected_) {
    statusBar()->showMessage("Select a component to clone.");
    return;
  }
  auto c = ComponentRegistry::instance().create(selected_->type);
  if (!c) return;
  c->fluid = selected_->fluid;
  c->params = selected_->params;
  c->curves = selected_->curves;
  c->x = selected_->x + 40;
  c->y = selected_->y + 40;
  net_.addComponent(std::move(c));  // gets a fresh id + tag
  scene_->rebuildFromNetwork();
  hierarchy_->refresh(&net_);
  statusBar()->showMessage("Cloned " + QString::fromStdString(selected_->name));
}

void MainWindow::saveInitialCondition() {
  QString path = QFileDialog::getSaveFileName(this, "Save Initial Condition",
                                              QString(), "Initial Condition (*.ic)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".ic")) path += ".ic";
  if (saveProject(net_, path.toStdString()))
    statusBar()->showMessage("Saved initial condition " + path);
}

void MainWindow::loadInitialCondition() {
  QString path = QFileDialog::getOpenFileName(this, "Load Initial Condition",
                                              QString(), "Initial Condition (*.ic)");
  if (path.isEmpty()) return;
  if (!loadProject(net_, path.toStdString())) {
    QMessageBox::warning(this, "Load IC", "Failed to load initial condition.");
    return;
  }
  scene_->rebuildFromNetwork();
  hierarchy_->refresh(&net_);
  sim_->captureInitial();
  sim_->initializeSteady();
  statusBar()->showMessage("Loaded initial condition " + path);
}

void MainWindow::openPlantData() {
  PlantDataDialog dlg(&net_, this);
  connect(&dlg, &PlantDataDialog::dataChanged, this, [this]() {
    scene_->update();
    hierarchy_->refresh(&net_);
    if (selected_) properties_->showComponent(selected_);  // keep panel in sync
  });
  dlg.exec();
  scene_->update();
  hierarchy_->refresh(&net_);
  if (selected_) properties_->showComponent(selected_);
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
  openPath(path);
}

void MainWindow::openPath(const QString& path) {
  if (!loadProject(net_, path.toStdString())) {
    QMessageBox::warning(this, "Open", "Failed to load project: " + path);
    return;
  }
  scene_->rebuildFromNetwork();
  scene_->clearRuntime();
  hierarchy_->refresh(&net_);
  properties_->showComponent(nullptr);
  sim_->captureInitial();
  statusBar()->showMessage("Opened " + path);
}

void MainWindow::startSimulation() { sim_->start(); }

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

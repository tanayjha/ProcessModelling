#include "gui/MainWindow.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <memory>

#include "core/ComponentRegistry.h"
#include "core/Project.h"
#include "gui/DebugDock.h"
#include "gui/DiagramScene.h"
#include "gui/HierarchyDock.h"
#include "gui/InstructorDialog.h"
#include "gui/PaletteDock.h"
#include "gui/PlantData.h"
#include "gui/ProjectDock.h"
#include "gui/PropertyEditor.h"
#include "gui/SimController.h"
#include "gui/TrendDock.h"
#include "solver/Validation.h"

namespace umpnap {

MainWindow::MainWindow() {
  registerHydraulicComponents();
  setWindowTitle("UMPNAP - Unified Multi-Domain Process Network Analysis Platform");
  resize(1280, 840);

  tabs_ = new QTabWidget(this);
  tabs_->setTabsClosable(true);
  tabs_->setDocumentMode(true);
  setCentralWidget(tabs_);
  connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
  connect(tabs_, &QTabWidget::tabCloseRequested, this,
          &MainWindow::onTabCloseRequested);

  palette_ = new PaletteDock(this);
  properties_ = new PropertyEditor(this);
  hierarchy_ = new HierarchyDock(this);
  project_ = new ProjectDock(this);
  debug_ = new DebugDock(this);

  addDockWidget(Qt::LeftDockWidgetArea, palette_);
  addDockWidget(Qt::LeftDockWidgetArea, project_);
  addDockWidget(Qt::LeftDockWidgetArea, hierarchy_);
  addDockWidget(Qt::RightDockWidgetArea, properties_);
  addDockWidget(Qt::RightDockWidgetArea, debug_);
  tabifyDockWidget(properties_, debug_);  // share the right column with Properties

  // The first (pristine) document; trends_ binds to its results.
  Document* first = addDocument(std::make_unique<Network>(), "Untitled", "");
  trends_ = new TrendDock(first->results.get(), this);
  addDockWidget(Qt::BottomDockWidgetArea, trends_);
  resizeDocks({trends_}, {300}, Qt::Vertical);

  // Palette placement and hierarchy navigation always target the active tab.
  connect(palette_, &PaletteDock::typeSelected, this, [this](const QString& t) {
    if (auto* d = activeDoc()) d->scene->setArmedType(t);
  });
  connect(hierarchy_, &HierarchyDock::componentActivated, this, [this](int id) {
    if (auto* d = activeDoc()) d->scene->selectComponent(id);
  });
  connect(properties_, &PropertyEditor::edited, this, [this]() {
    if (auto* d = activeDoc()) {
      d->scene->update();
      hierarchy_->refresh(d->net.get());
    }
  });
  connect(project_, &ProjectDock::documentActivated, this, [this](int idx) {
    if (idx >= 0 && idx < tabs_->count()) tabs_->setCurrentIndex(idx);
  });

  buildMenus();
  buildSimToolbar();
  bindActiveDocument();
  statusBar()->showMessage(
      "Ready. Each mimic opens on its own tab; open a Plant Project to develop "
      "subsystems independently and run them linked on the Integrated tab.");
}

// --------------------------- document management ---------------------------

Document* MainWindow::addDocument(std::unique_ptr<Network> net,
                                  const QString& title, const QString& path,
                                  bool integrated) {
  auto doc = std::make_unique<Document>();
  doc->net = std::move(net);
  doc->results = std::make_unique<Results>();
  doc->title = title;
  doc->path = path;
  doc->integrated = integrated;
  doc->view = new QGraphicsView(this);
  doc->scene = new DiagramScene(doc->net.get(), doc->view);
  doc->view->setScene(doc->scene);
  doc->view->setRenderHint(QPainter::Antialiasing, true);
  doc->view->setDragMode(QGraphicsView::RubberBandDrag);
  doc->sim = new SimController(doc->net.get(), doc->results.get(), this);

  Document* d = doc.get();
  connect(d->scene, &DiagramScene::componentSelected, this, [this, d](Component* c) {
    d->selected = c;
    if (d == activeDoc()) {
      properties_->showComponent(c);
      debug_->showComponent(c, d->net.get());
      debug_->refresh(*d->results);
    }
  });
  connect(d->scene, &DiagramScene::networkChanged, this, [this, d]() {
    bankUndo(d);
    if (d == activeDoc()) hierarchy_->refresh(d->net.get());
  });
  connect(d->scene, &DiagramScene::connectionRejected, this,
          [this](const QString& r) { statusBar()->showMessage(r, 5000); });
  connect(d->sim, &SimController::updated, this, [this, d]() { onSimUpdated(d); });
  connect(d->sim, &SimController::modeChanged, this,
          [this, d]() { onSimModeChanged(d); });

  d->scene->rebuildFromNetwork();
  d->sim->captureInitial();
  d->lastSnapshot = d->net->clone();  // undo baseline

  docs_.push_back(std::move(doc));
  QString label = (integrated ? QString("▣ ") : QString()) + title;
  int idx = tabs_->addTab(d->view, label);
  tabs_->setCurrentIndex(idx);
  refreshProjectDock();
  return d;
}

Document* MainWindow::activeDoc() {
  int i = tabs_->currentIndex();
  return (i >= 0 && i < (int)docs_.size()) ? docs_[i].get() : nullptr;
}

SimController* MainWindow::activeSim() {
  Document* d = activeDoc();
  return d ? d->sim : nullptr;
}

void MainWindow::bindActiveDocument() {
  Document* d = activeDoc();
  if (!d) return;
  properties_->showComponent(d->selected);
  hierarchy_->refresh(d->net.get());
  if (trends_) {
    trends_->setNetwork(d->net.get());
    trends_->setResults(d->results.get());
  }
  updateClock(d);
  // Bring the diagram into view (loaded components sit at saved coordinates).
  QRectF r = d->scene->itemsBoundingRect();
  if (!r.isEmpty()) {
    d->view->setSceneRect(r.adjusted(-300, -300, 300, 300));
    d->view->centerOn(r.center());
  }
}

void MainWindow::refreshProjectDock() {
  QStringList titles;
  QList<bool> integrated;
  QList<int> parentOf;
  for (size_t i = 0; i < docs_.size(); ++i) {
    titles << docs_[i]->title;
    integrated << docs_[i]->integrated;
    parentOf << -1;
  }
  // Nest each integrated plant's members beneath it.
  for (size_t i = 0; i < docs_.size(); ++i) {
    if (!docs_[i]->integrated) continue;
    for (Document* m : docs_[i]->members) {
      for (size_t j = 0; j < docs_.size(); ++j)
        if (docs_[j].get() == m) parentOf[(int)j] = (int)i;
    }
  }
  project_->setDocuments(titles, integrated, parentOf, tabs_->currentIndex());
}

void MainWindow::rebuildIntegrated(Document* doc) {
  if (!doc || !doc->integrated) return;
  std::vector<const Network*> nets;
  for (Document* m : doc->members) nets.push_back(m->net.get());
  *doc->net = mergeMimics(nets);
  doc->scene->rebuildFromNetwork();
  doc->scene->clearRuntime();
  doc->sim->captureInitial();
}

// ------------------------------- sim wiring --------------------------------

void MainWindow::onSimUpdated(Document* d) {
  d->scene->updateRuntime(*d->results);
  if (d == activeDoc()) {
    trends_->liveUpdate();
    debug_->refresh(*d->results);
    updateClock(d);
  }
}

void MainWindow::onSimModeChanged(Document* d) {
  onSimUpdated(d);
  // Lock topology edits (delete of components/wires) while the engine runs.
  d->scene->setEditable(d->sim->mode() != SimController::Running);
  if (d != activeDoc()) return;
  const char* m = d->sim->mode() == SimController::Running ? "Running"
                  : d->sim->mode() == SimController::Paused
                      ? "Paused (inspect/edit allowed)"
                      : "Stopped";
  statusBar()->showMessage(QString("Simulation: %1").arg(m));
}

void MainWindow::updateClock(Document* d) {
  if (!clock_ || !d) return;
  const char* m = d->sim->mode() == SimController::Running ? "Running"
                  : d->sim->mode() == SimController::Paused ? "Paused"
                                                            : "Stopped";
  clock_->setText(
      QString("  t = %1 s   [%2]  ").arg(d->sim->time(), 0, 'f', 1).arg(m));
}

void MainWindow::onTabChanged(int) {
  Document* d = activeDoc();
  if (d && d->integrated) rebuildIntegrated(d);  // reflect latest member edits
  bindActiveDocument();
  refreshProjectDock();
}

void MainWindow::onTabCloseRequested(int index) {
  if (index < 0 || index >= (int)docs_.size()) return;
  // Removing an integrated plant's member would orphan the merge; block it.
  Document* victim = docs_[index].get();
  for (const auto& dp : docs_)
    if (dp->integrated)
      for (Document* m : dp->members)
        if (m == victim) {
          statusBar()->showMessage(
              "Close the Integrated tab before its member mimics.");
          return;
        }
  tabs_->removeTab(index);
  docs_.erase(docs_.begin() + index);
  if (docs_.empty())
    addDocument(std::make_unique<Network>(), "Untitled", "");
  refreshProjectDock();
}

// --------------------------------- menus -----------------------------------

void MainWindow::buildMenus() {
  QMenu* file = menuBar()->addMenu("&File");
  file->addAction("&New Tab", this, &MainWindow::newProject);
  file->addAction("&Open Mimic...", this, &MainWindow::openProject);
  file->addAction("Open &Plant Project...", this, [this]() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open Plant Project", QString(), "UMPNAP Plant (*.umpproj)");
    if (!path.isEmpty()) openPlantPath(path);
  });
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
  edit->addAction("&Undo", QKeySequence::Undo, this, &MainWindow::undo);
  edit->addSeparator();
  edit->addAction("&Plant Data...", this, &MainWindow::openPlantData);
  edit->addAction("&Clone Selected", QKeySequence("Ctrl+D"), this,
                  &MainWindow::cloneSelected);

  QMenu* sim = menuBar()->addMenu("&Simulation");
  sim->addAction("&Initialize (Steady)", this,
                 [this]() { if (auto* s = activeSim()) s->initializeSteady(); });
  sim->addAction("&Run", this, [this]() { if (auto* s = activeSim()) s->start(); });
  sim->addAction("&Pause / Freeze", this,
                 [this]() { if (auto* s = activeSim()) s->pause(); });
  sim->addAction("Single &Step", this,
                 [this]() { if (auto* s = activeSim()) s->singleStep(); });
  sim->addAction("Rese&t", this, [this]() { if (auto* s = activeSim()) s->reset(); });
  sim->addSeparator();
  sim->addSeparator();
  sim->addAction("&Instructor Station (Malfunctions)...", this, [this]() {
    Document* d = activeDoc();
    if (!d || d->integrated) {
      statusBar()->showMessage("Open an editable mimic to set malfunctions.", 3000);
      return;
    }
    InstructorDialog dlg(d->net.get(), this);
    connect(&dlg, &InstructorDialog::changed, this, [this, d]() {
      // Apply immediately: re-solve steady so the malfunction/override shows,
      // and refresh overlays/debug. During a live run the next cycle picks it up.
      if (d->sim->mode() != SimController::Running) {
        d->sim->initializeSteady();
        onSimUpdated(d);
      }
    });
    dlg.exec();
  });
  sim->addAction("Rebuild &Integrated Plant", this, [this]() {
    Document* d = activeDoc();
    if (d && d->integrated) {
      rebuildIntegrated(d);
      statusBar()->showMessage("Re-merged member mimics into the integrated run.");
    } else {
      statusBar()->showMessage("Switch to an Integrated plant tab first.");
    }
  });
  sim->addSeparator();
  sim->addAction("Save Snapshot", this,
                 [this]() { if (auto* s = activeSim()) s->saveSnapshot(); });
  sim->addAction("Restore Snapshot", this,
                 [this]() { if (auto* s = activeSim()) s->restoreSnapshot(); });
  sim->addAction("Set &Timestep...", this, [this]() {
    auto* s = activeSim();
    if (!s) return;
    bool ok = false;
    double dt = QInputDialog::getDouble(this, "Timestep", "dt (s):", s->dt(),
                                        1e-6, 1e6, 4, &ok);
    if (ok) s->setDt(dt);
  });

  QMenu* view = menuBar()->addMenu("&View");
  for (QDockWidget* d : {static_cast<QDockWidget*>(palette_),
                         static_cast<QDockWidget*>(project_),
                         static_cast<QDockWidget*>(hierarchy_),
                         static_cast<QDockWidget*>(properties_),
                         static_cast<QDockWidget*>(debug_),
                         static_cast<QDockWidget*>(trends_)})
    if (d) view->addAction(d->toggleViewAction());
  view->addSeparator();
  view->addAction("Restore All Panels", this, [this]() {
    for (QDockWidget* d : {static_cast<QDockWidget*>(palette_),
                           static_cast<QDockWidget*>(project_),
                           static_cast<QDockWidget*>(hierarchy_),
                           static_cast<QDockWidget*>(properties_),
                           static_cast<QDockWidget*>(trends_)}) {
      d->show();
      d->setFloating(false);
    }
    addDockWidget(Qt::LeftDockWidgetArea, palette_);
    addDockWidget(Qt::LeftDockWidgetArea, project_);
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
  tb->addAction("⏮ Init", this,
                [this]() { if (auto* s = activeSim()) s->initializeSteady(); });
  tb->addAction("▶ Run", this, [this]() { if (auto* s = activeSim()) s->start(); });
  tb->addAction("⏸ Pause", this, [this]() { if (auto* s = activeSim()) s->pause(); });
  tb->addAction("⏭ Step", this,
                [this]() { if (auto* s = activeSim()) s->singleStep(); });
  tb->addAction("⟲ Reset", this, [this]() { if (auto* s = activeSim()) s->reset(); });
  tb->addSeparator();
  tb->addWidget(new QLabel(" Speed ", tb));
  auto* speed = new QComboBox(tb);
  speed->addItems({"1x", "2x", "5x", "10x", "20x"});
  connect(speed, &QComboBox::currentTextChanged, this, [this](const QString& s) {
    if (auto* sc = activeSim()) sc->setSpeed(s.left(s.size() - 1).toInt());
  });
  tb->addWidget(speed);
  tb->addSeparator();
  tb->addAction("Snapshot", this,
                [this]() { if (auto* s = activeSim()) s->saveSnapshot(); });
  tb->addAction("Restore", this,
                [this]() { if (auto* s = activeSim()) s->restoreSnapshot(); });
  tb->addAction("Validate", this, &MainWindow::runValidation);
  tb->addSeparator();
  tb->addWidget(new QLabel(" Find tag ", tb));
  auto* search = new QLineEdit(tb);
  search->setPlaceholderText("e.g. P-2");
  search->setMaximumWidth(120);
  search->setClearButtonEnabled(true);
  connect(search, &QLineEdit::returnPressed, this, [this, search]() {
    Document* d = activeDoc();
    if (!d) return;
    QString q = search->text().trimmed();
    if (q.isEmpty()) return;
    Component* c = d->net->componentByName(q.toStdString());
    if (!c) {
      for (const auto& comp : d->net->components())
        if (QString::fromStdString(comp->name).contains(q, Qt::CaseInsensitive)) {
          c = comp.get();
          break;
        }
    }
    if (c) {
      d->scene->selectComponent(c->id);
      d->view->centerOn(c->x, c->y);
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

// ------------------------------ file actions -------------------------------

void MainWindow::cloneSelected() {
  Document* d = activeDoc();
  if (!d || !d->selected) {
    statusBar()->showMessage("Select a component to clone.");
    return;
  }
  auto c = ComponentRegistry::instance().create(d->selected->type);
  if (!c) return;
  c->fluid = d->selected->fluid;
  c->params = d->selected->params;
  c->curves = d->selected->curves;
  c->config = d->selected->config;
  c->x = d->selected->x + 40;
  c->y = d->selected->y + 40;
  QString name = QString::fromStdString(d->selected->name);
  d->net->addComponent(std::move(c));
  bankUndo(d);
  d->scene->rebuildFromNetwork();
  hierarchy_->refresh(d->net.get());
  statusBar()->showMessage("Cloned " + name);
}

// Bank the pre-edit state for undo. Called after each committed topology edit;
// `lastSnapshot` holds the state prior to this edit, so it becomes the target an
// undo restores. Editable mimics only — the integrated tab is regenerated.
void MainWindow::bankUndo(Document* d) {
  if (!d || d->integrated) return;
  if (d->lastSnapshot) d->undoStack.push_back(std::move(d->lastSnapshot));
  d->lastSnapshot = d->net->clone();
  if (d->undoStack.size() > 100) d->undoStack.erase(d->undoStack.begin());
}

void MainWindow::undo() {
  Document* d = activeDoc();
  if (!d || d->integrated || d->undoStack.empty()) {
    statusBar()->showMessage("Nothing to undo.", 2000);
    return;
  }
  d->net->copyFrom(*d->undoStack.back());
  d->undoStack.pop_back();
  d->lastSnapshot = d->net->clone();
  d->selected = nullptr;
  d->scene->rebuildFromNetwork();   // does not emit networkChanged
  hierarchy_->refresh(d->net.get());
  properties_->showComponent(nullptr);
  statusBar()->showMessage("Undid last edit.", 3000);
}

void MainWindow::saveInitialCondition() {
  Document* d = activeDoc();
  if (!d) return;
  QString path = QFileDialog::getSaveFileName(this, "Save Initial Condition",
                                              QString(), "Initial Condition (*.ic)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".ic")) path += ".ic";
  if (saveProject(*d->net, path.toStdString()))
    statusBar()->showMessage("Saved initial condition " + path);
}

void MainWindow::loadInitialCondition() {
  Document* d = activeDoc();
  if (!d) return;
  QString path = QFileDialog::getOpenFileName(this, "Load Initial Condition",
                                              QString(), "Initial Condition (*.ic)");
  if (path.isEmpty()) return;
  if (!loadProject(*d->net, path.toStdString())) {
    QMessageBox::warning(this, "Load IC", "Failed to load initial condition.");
    return;
  }
  d->scene->rebuildFromNetwork();
  hierarchy_->refresh(d->net.get());
  d->sim->captureInitial();
  d->sim->initializeSteady();
  statusBar()->showMessage("Loaded initial condition " + path);
}

void MainWindow::openPlantData() {
  Document* d = activeDoc();
  if (!d) return;
  PlantDataDialog dlg(d->net.get(), this);
  connect(&dlg, &PlantDataDialog::dataChanged, this, [this, d]() {
    d->scene->update();
    hierarchy_->refresh(d->net.get());
    if (d->selected) properties_->showComponent(d->selected);
  });
  dlg.exec();
  d->scene->update();
  hierarchy_->refresh(d->net.get());
  if (d->selected) properties_->showComponent(d->selected);
}

void MainWindow::newProject() {
  addDocument(std::make_unique<Network>(), "Untitled", "");
  statusBar()->showMessage("New tab.");
}

void MainWindow::openProject() {
  QString path = QFileDialog::getOpenFileName(this, "Open Mimic", QString(),
                                              "UMPNAP Projects (*.umpnap)");
  if (path.isEmpty()) return;
  openPath(path);
}

void MainWindow::openPath(const QString& path) {
  auto net = std::make_unique<Network>();
  if (!loadProject(*net, path.toStdString())) {
    QMessageBox::warning(this, "Open", "Failed to load project: " + path);
    return;
  }
  QString title = QFileInfo(path).completeBaseName();
  // Reuse the initial pristine tab if it is still empty and unsaved.
  Document* d = activeDoc();
  if (docs_.size() == 1 && d && d->path.isEmpty() && d->net->components().empty()) {
    *d->net = std::move(*net);
    d->title = title;
    d->path = path;
    d->scene->rebuildFromNetwork();
    d->scene->clearRuntime();
    d->sim->captureInitial();
    d->undoStack.clear();
    d->lastSnapshot = d->net->clone();  // fresh undo baseline for the loaded net
    tabs_->setTabText(0, title);
    bindActiveDocument();
    refreshProjectDock();
  } else {
    addDocument(std::move(net), title, path);
  }
  statusBar()->showMessage("Opened " + path);
}

void MainWindow::openPlantPath(const QString& path) {
  PlantProject proj;
  if (!loadPlant(proj, path.toStdString())) {
    QMessageBox::warning(this, "Open Plant",
                         "Failed to load plant project: " + path);
    return;
  }
  QString dir = QFileInfo(path).absolutePath() + "/";
  std::vector<Document*> members;
  for (const std::string& rel : proj.mimics) {
    QString full = QString::fromStdString(rel);
    if (!full.startsWith("/")) full = dir + full;
    auto net = std::make_unique<Network>();
    if (!loadProject(*net, full.toStdString())) continue;
    Document* m = addDocument(std::move(net), QFileInfo(full).completeBaseName(),
                              full);
    members.push_back(m);
  }
  // Build the integrated (merged) run tab from the live member networks.
  std::vector<const Network*> nets;
  for (Document* m : members) nets.push_back(m->net.get());
  QString plantTitle =
      proj.name.empty() ? QFileInfo(path).completeBaseName()
                        : QString::fromStdString(proj.name);
  Document* integ = addDocument(std::make_unique<Network>(mergeMimics(nets)),
                                plantTitle, path, /*integrated=*/true);
  integ->members = members;
  rebuildIntegrated(integ);  // re-merge now that members are wired
  bindActiveDocument();      // integrated tab is already current; bind its docks
  refreshProjectDock();
  statusBar()->showMessage(
      "Opened plant '" + plantTitle +
      "': edit each mimic on its tab; the Integrated tab runs them linked.");
}

void MainWindow::startSimulation() {
  if (auto* s = activeSim()) s->start();
}

void MainWindow::saveProject_() {
  Document* d = activeDoc();
  if (!d) return;
  QString path = d->path;
  if (path.isEmpty() || d->integrated)
    path = QFileDialog::getSaveFileName(this, "Save Mimic", d->path,
                                        "UMPNAP Projects (*.umpnap)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".umpnap")) path += ".umpnap";
  if (!saveProject(*d->net, path.toStdString())) {
    QMessageBox::warning(this, "Save", "Failed to save project.");
    return;
  }
  d->path = path;
  d->title = QFileInfo(path).completeBaseName();
  if (!d->integrated) tabs_->setTabText(tabs_->currentIndex(), d->title);
  refreshProjectDock();
  statusBar()->showMessage("Saved " + path);
}

void MainWindow::exportCsv() {
  Document* d = activeDoc();
  if (!d) return;
  QString path = QFileDialog::getSaveFileName(this, "Export Results", QString(),
                                              "CSV (*.csv)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".csv")) path += ".csv";
  if (!d->results->writeCsv(path.toStdString())) {
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
      "Validated single-phase hydraulic + pneumatic nodal solver, a compressible "
      "steam pressure-flow solver, a linear DC electrical power-flow solver, and "
      "split shell-and-tube thermal coupling. Multi-mimic plant projects develop "
      "subsystems on independent tabs and simulate them linked on an integrated "
      "tab.");
}

}  // namespace umpnap

#pragma once
#include <QMainWindow>
#include <QString>
#include <memory>
#include <vector>

#include "core/Network.h"
#include "core/Results.h"

class QGraphicsView;
class QTabWidget;
class QLabel;

namespace umpnap {

class DiagramScene;
class PaletteDock;
class PropertyEditor;
class HierarchyDock;
class TrendDock;
class ProjectDock;
class SimController;
class Component;

// One editable mimic, or the merged integrated-plant run, shown on its own tab.
// Each document owns its network, results, scene/view and simulation engine, so
// different subsystems are developed independently and linked into one run.
struct Document {
  std::unique_ptr<Network> net;
  std::unique_ptr<Results> results;
  DiagramScene* scene = nullptr;     // parented to view
  QGraphicsView* view = nullptr;     // the tab page widget
  SimController* sim = nullptr;
  Component* selected = nullptr;
  QString title;
  QString path;                      // .umpnap path, or empty if unsaved
  bool integrated = false;           // the merged plant run tab
  std::vector<Document*> members;    // integrated: the member mimic documents

  // Undo: each committed edit pushes the prior network state. `lastSnapshot`
  // mirrors the current committed state so the next edit can bank it.
  std::vector<std::unique_ptr<Network>> undoStack;
  std::unique_ptr<Network> lastSnapshot;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow();

  // Open a single mimic (.umpnap) on its own tab.
  void openPath(const QString& path);
  // Open a multi-mimic plant project (.umpproj): one editable tab per member
  // mimic plus an integrated tab that merges them and simulates in unison.
  void openPlantPath(const QString& path);
  // Start the active document's simulation engine.
  void startSimulation();

 private slots:
  void newProject();
  void openProject();
  void saveProject_();
  void exportCsv();
  void runValidation();
  void openPlantData();
  void cloneSelected();
  void undo();
  void saveInitialCondition();
  void loadInitialCondition();
  void about();
  void onTabChanged(int index);
  void onTabCloseRequested(int index);

 private:
  void buildMenus();
  void buildSimToolbar();
  Document* addDocument(std::unique_ptr<Network> net, const QString& title,
                        const QString& path, bool integrated = false);
  Document* activeDoc();
  SimController* activeSim();
  void bindActiveDocument();
  void bankUndo(Document* d);  // push prior state onto the doc's undo stack
  void refreshProjectDock();
  void rebuildIntegrated(Document* doc);
  void onSimUpdated(Document* d);
  void onSimModeChanged(Document* d);
  void updateClock(Document* d);

  QTabWidget* tabs_ = nullptr;
  std::vector<std::unique_ptr<Document>> docs_;  // aligned with tab index

  PaletteDock* palette_ = nullptr;
  PropertyEditor* properties_ = nullptr;
  HierarchyDock* hierarchy_ = nullptr;
  TrendDock* trends_ = nullptr;
  ProjectDock* project_ = nullptr;
  QLabel* clock_ = nullptr;
};

}  // namespace umpnap

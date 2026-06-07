#pragma once
#include <QMainWindow>

#include "core/Network.h"
#include "core/Results.h"
#include "solver/SolverManager.h"

class QGraphicsView;

namespace umpnap {

class DiagramScene;
class PaletteDock;
class PropertyEditor;
class HierarchyDock;
class TrendDock;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow();

 private slots:
  void newProject();
  void openProject();
  void saveProject_();
  void exportCsv();
  void runSteady();
  void runTransient();
  void runValidation();
  void openPlantData();
  void about();

 private:
  void buildMenus();

  Network net_;
  Results results_;
  SolverManager solver_;

  DiagramScene* scene_ = nullptr;
  QGraphicsView* view_ = nullptr;
  PaletteDock* palette_ = nullptr;
  PropertyEditor* properties_ = nullptr;
  HierarchyDock* hierarchy_ = nullptr;
  TrendDock* trends_ = nullptr;
};

}  // namespace umpnap

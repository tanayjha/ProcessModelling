#pragma once
#include <QMainWindow>

#include "core/Network.h"
#include "core/Results.h"
#include "solver/SolverManager.h"

class QGraphicsView;
class QLabel;
class QComboBox;

namespace umpnap {

class DiagramScene;
class PaletteDock;
class PropertyEditor;
class HierarchyDock;
class TrendDock;
class SimController;
class Component;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow();

 private slots:
  void newProject();
  void openProject();
  void saveProject_();
  void exportCsv();
  void runValidation();
  void openPlantData();
  void cloneSelected();
  void saveInitialCondition();
  void loadInitialCondition();
  void onSimUpdated();
  void onSimModeChanged();
  void about();

 private:
  void buildMenus();
  void buildSimToolbar();

  Network net_;
  Results results_;
  SolverManager solver_;
  SimController* sim_ = nullptr;
  Component* selected_ = nullptr;

  DiagramScene* scene_ = nullptr;
  QGraphicsView* view_ = nullptr;
  PaletteDock* palette_ = nullptr;
  PropertyEditor* properties_ = nullptr;
  HierarchyDock* hierarchy_ = nullptr;
  TrendDock* trends_ = nullptr;
  QLabel* clock_ = nullptr;
};

}  // namespace umpnap

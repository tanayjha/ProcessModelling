#pragma once
#include <QDockWidget>

class QTableWidget;

namespace umpnap {

class Component;
class Network;
class Results;

// Live "debug" introspection panel: when a component is selected, shows every
// value the simulation calculates for it at the current step (branch flow, head,
// position, per-port node pressures, tank level, ...), refreshed each cycle.
// Complements the plant-data sheet, which shows design *inputs*; this shows the
// computed *outputs* — the dynamic state engineers expect from a simulator.
class DebugDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit DebugDock(QWidget* parent = nullptr);
  // Set the component under inspection (and the network it lives in).
  void showComponent(Component* c, Network* net);
  // Repopulate the calculated values from the latest results.
  void refresh(const Results& res);

 private:
  QTableWidget* table_ = nullptr;
  Component* comp_ = nullptr;
  Network* net_ = nullptr;
};

}  // namespace umpnap

#include "gui/PaletteDock.h"

#include <QTreeWidget>
#include <map>
#include <set>
#include <string>

#include "core/ComponentRegistry.h"

namespace umpnap {

namespace {
// Library group a component type belongs to.
QString libraryOf(const ComponentDef& d) {
  static const std::set<std::string> air = {"Duct",      "Damper",
                                             "Fan",       "Blower",
                                             "Compressor", "AirReceiver",
                                             "GasReliefValve", "NonReturnValve"};
  static const std::set<std::string> steam = {"SteamGenerator", "Turbine",
                                              "Condenser", "Deaerator",
                                              "ASDV", "CSDV"};
  static const std::set<std::string> heat = {"HeatExchanger", "ShellSide",
                                             "TubeSide"};
  if (d.domain == Domain::Electrical) return "Electrical";
  if (d.domain == Domain::Instrument || d.domain == Domain::Control)
    return "Instrumentation & Control";
  if (steam.count(d.type)) return "Steam";
  if (air.count(d.type)) return "Air / Gas";
  if (heat.count(d.type)) return "Heat Transfer";
  return "Hydraulic";
}

// Display order of the groups.
int groupOrder(const QString& g) {
  if (g == "Hydraulic") return 0;
  if (g == "Heat Transfer") return 1;
  if (g == "Air / Gas") return 2;
  if (g == "Steam") return 3;
  if (g == "Electrical") return 4;
  return 5;  // Instrumentation & Control
}
}  // namespace

PaletteDock::PaletteDock(QWidget* parent) : QDockWidget("Component Palette", parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  setWidget(tree_);

  // Bucket types by library, preserving registry order within each group.
  std::map<QString, QTreeWidgetItem*> groups;
  auto groupItem = [&](const QString& name) -> QTreeWidgetItem* {
    auto it = groups.find(name);
    if (it != groups.end()) return it->second;
    auto* g = new QTreeWidgetItem(tree_, {name});
    g->setFlags(Qt::ItemIsEnabled);  // header: not selectable/armable
    g->setExpanded(true);
    groups[name] = g;
    return g;
  };

  // Ensure groups appear in a stable, sensible order.
  for (const char* g : {"Hydraulic", "Heat Transfer", "Air / Gas", "Steam",
                        "Electrical", "Instrumentation & Control"})
    groupItem(g);

  for (const auto& def : ComponentRegistry::instance().defs()) {
    QString lib = libraryOf(def);
    auto* leaf = new QTreeWidgetItem(groupItem(lib),
                                     {QString::fromStdString(def.type)});
    leaf->setData(0, Qt::UserRole, QString::fromStdString(def.type));
  }
  (void)groupOrder;

  connect(tree_, &QTreeWidget::itemClicked, this, &PaletteDock::onItemClicked);
}

void PaletteDock::onItemClicked(QTreeWidgetItem* item, int) {
  QVariant v = item->data(0, Qt::UserRole);
  if (v.isValid()) emit typeSelected(v.toString());  // ignore group headers
}

}  // namespace umpnap

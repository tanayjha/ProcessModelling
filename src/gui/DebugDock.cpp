#include "gui/DebugDock.h"

#include <QHeaderView>
#include <QTableWidget>
#include <string>
#include <vector>

#include "core/Network.h"
#include "core/Results.h"
#include "solver/NodeGraph.h"

namespace umpnap {

DebugDock::DebugDock(QWidget* parent) : QDockWidget("Debug (calculated)", parent) {
  table_ = new QTableWidget(this);
  table_->setColumnCount(2);
  table_->setHorizontalHeaderLabels({"Calculated value", "Now"});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->verticalHeader()->setVisible(false);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  setWidget(table_);
}

void DebugDock::showComponent(Component* c, Network* net) {
  comp_ = c;
  net_ = net;
  table_->setRowCount(0);
}

namespace {
void addRow(QTableWidget* t, const QString& k, const QString& v) {
  int r = t->rowCount();
  t->insertRow(r);
  t->setItem(r, 0, new QTableWidgetItem(k));
  t->setItem(r, 1, new QTableWidgetItem(v));
}
}  // namespace

void DebugDock::refresh(const Results& res) {
  table_->setRowCount(0);
  if (!comp_ || !net_) return;

  addRow(table_, "tag", QString::fromStdString(comp_->name));
  addRow(table_, "type", QString::fromStdString(comp_->type));

  const std::string pfx = "comp." + std::to_string(comp_->id) + ".";
  for (const auto& key : res.keys()) {
    if (key.rfind(pfx, 0) != 0) continue;
    QString label = QString::fromStdString(key.substr(pfx.size()));
    addRow(table_, label, QString::number(res.latest(key), 'g', 6));
  }

  // Per-port node pressures (kPa) so junction/header/tank/boundary nodes show.
  NodeGraph g = buildNodeGraph(*net_);
  for (const auto& p : comp_->ports) {
    int n = g.nodeOf(comp_->id, p.name);
    if (n < 0) continue;
    double pa = res.latest("node." + std::to_string(n) + ".pressure");
    if (pa == 0.0) continue;
    addRow(table_, QString("P(%1)  kPa").arg(QString::fromStdString(p.name)),
           QString::number(pa / 1000.0, 'f', 2));
  }

  if (comp_->type == "Tank" || comp_->type == "PressurizedTank" ||
      comp_->type == "AirReceiver")
    addRow(table_, "level  m", QString::number(comp_->param("level"), 'f', 3));

  table_->resizeColumnsToContents();
}

}  // namespace umpnap

#include "gui/HierarchyDock.h"

#include <QTreeWidget>
#include <map>

#include "core/Network.h"

namespace umpnap {

HierarchyDock::HierarchyDock(QWidget* parent) : QDockWidget("Model Hierarchy", parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabel("Components");
  setWidget(tree_);
  connect(tree_, &QTreeWidget::itemClicked, this, &HierarchyDock::onItemClicked);
}

void HierarchyDock::refresh(Network* net) {
  tree_->clear();
  std::map<std::string, QTreeWidgetItem*> domainNodes;
  for (const auto& c : net->components()) {
    std::string dom = domainName(c->domain);
    auto it = domainNodes.find(dom);
    if (it == domainNodes.end()) {
      auto* dn = new QTreeWidgetItem(tree_, {QString::fromStdString(dom)});
      dn->setExpanded(true);
      domainNodes[dom] = dn;
      it = domainNodes.find(dom);
    }
    auto* node = new QTreeWidgetItem(
        it->second, {QString::fromStdString(c->type) + " #" + QString::number(c->id)});
    node->setData(0, Qt::UserRole, c->id);
  }
  tree_->expandAll();
}

void HierarchyDock::onItemClicked(QTreeWidgetItem* item, int) {
  QVariant v = item->data(0, Qt::UserRole);
  if (v.isValid()) emit componentActivated(v.toInt());
}

}  // namespace umpnap

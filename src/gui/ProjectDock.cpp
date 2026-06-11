#include "gui/ProjectDock.h"

#include <QTreeWidget>
#include <map>

namespace umpnap {

ProjectDock::ProjectDock(QWidget* parent) : QDockWidget("Project", parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  setWidget(tree_);
  connect(tree_, &QTreeWidget::itemClicked, this, &ProjectDock::onActivated);
}

void ProjectDock::setDocuments(const QStringList& titles,
                               const QList<bool>& integrated,
                               const QList<int>& parentOf, int active) {
  tree_->clear();
  std::map<int, QTreeWidgetItem*> byTab;
  // First pass: top-level entries (parentOf < 0).
  for (int i = 0; i < titles.size(); ++i) {
    if (i < parentOf.size() && parentOf[i] >= 0) continue;
    auto* it = new QTreeWidgetItem(tree_, {titles[i]});
    it->setData(0, Qt::UserRole, i);
    if (i < integrated.size() && integrated[i]) {
      QFont f = it->font(0);
      f.setBold(true);
      it->setFont(0, f);
    }
    it->setExpanded(true);
    byTab[i] = it;
  }
  // Second pass: nested members.
  for (int i = 0; i < titles.size(); ++i) {
    if (i >= parentOf.size() || parentOf[i] < 0) continue;
    QTreeWidgetItem* parent = byTab.count(parentOf[i]) ? byTab[parentOf[i]] : nullptr;
    auto* it = parent ? new QTreeWidgetItem(parent, {titles[i]})
                      : new QTreeWidgetItem(tree_, {titles[i]});
    it->setData(0, Qt::UserRole, i);
    byTab[i] = it;
    if (parent) parent->setExpanded(true);
  }
  if (active >= 0 && byTab.count(active)) tree_->setCurrentItem(byTab[active]);
}

void ProjectDock::onActivated(QTreeWidgetItem* item, int) {
  if (!item) return;
  QVariant v = item->data(0, Qt::UserRole);
  if (v.isValid()) emit documentActivated(v.toInt());
}

}  // namespace umpnap

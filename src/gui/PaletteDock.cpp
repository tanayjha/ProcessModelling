#include "gui/PaletteDock.h"

#include <QListWidget>

#include "core/ComponentRegistry.h"

namespace umpnap {

PaletteDock::PaletteDock(QWidget* parent) : QDockWidget("Component Palette", parent) {
  list_ = new QListWidget(this);
  for (const auto& def : ComponentRegistry::instance().defs()) {
    auto* item = new QListWidgetItem(
        QString::fromStdString(def.type) + "  (" + domainName(def.domain) + ")");
    item->setData(Qt::UserRole, QString::fromStdString(def.type));
    list_->addItem(item);
  }
  setWidget(list_);
  connect(list_, &QListWidget::itemClicked, this, &PaletteDock::onItemClicked);
}

void PaletteDock::onItemClicked(QListWidgetItem* item) {
  emit typeSelected(item->data(Qt::UserRole).toString());
}

}  // namespace umpnap

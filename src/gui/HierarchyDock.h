#pragma once
#include <QDockWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace umpnap {

class Network;

// Tree of components grouped by domain. Selecting an entry selects on canvas.
class HierarchyDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit HierarchyDock(QWidget* parent = nullptr);
  void refresh(Network* net);

 signals:
  void componentActivated(int id);

 private slots:
  void onItemClicked(QTreeWidgetItem* item, int column);

 private:
  QTreeWidget* tree_;
};

}  // namespace umpnap

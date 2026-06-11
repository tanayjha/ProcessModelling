#pragma once
#include <QDockWidget>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;

namespace umpnap {

// Project explorer: lists every open document (member mimics and the integrated
// plant run) so the user can jump between canvas tabs. Integrated documents are
// shown with their member mimics nested beneath them.
class ProjectDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit ProjectDock(QWidget* parent = nullptr);

  // titles[i] is the tab title for document i; integrated[i] flags the merged
  // plant tab; parentOf[i] is the tab index this document belongs under (e.g. a
  // member nested below its integrated plant), or -1 for a top-level entry.
  void setDocuments(const QStringList& titles, const QList<bool>& integrated,
                    const QList<int>& parentOf, int active);

 signals:
  void documentActivated(int tabIndex);

 private slots:
  void onActivated(QTreeWidgetItem* item, int column);

 private:
  QTreeWidget* tree_;
};

}  // namespace umpnap

#pragma once
#include <QDockWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace umpnap {

// Component palette grouped by library (Hydraulic, Air/Gas, Steam, Electrical,
// Instrumentation & Control). Clicking a type arms it for placement.
class PaletteDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit PaletteDock(QWidget* parent = nullptr);

 signals:
  void typeSelected(const QString& type);

 private slots:
  void onItemClicked(QTreeWidgetItem* item, int column);

 private:
  QTreeWidget* tree_;
};

}  // namespace umpnap

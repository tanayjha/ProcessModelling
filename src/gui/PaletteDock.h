#pragma once
#include <QDockWidget>

class QListWidget;
class QListWidgetItem;

namespace umpnap {

// Lists registered component types; clicking one arms it for placement.
class PaletteDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit PaletteDock(QWidget* parent = nullptr);

 signals:
  void typeSelected(const QString& type);

 private slots:
  void onItemClicked(QListWidgetItem* item);

 private:
  QListWidget* list_;
};

}  // namespace umpnap

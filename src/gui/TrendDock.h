#pragma once
#include <QDockWidget>

class QListWidget;

namespace umpnap {

class Results;
class TrendWidget;

// Signal picker + trend plot. Reads from a Results owned elsewhere.
class TrendDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit TrendDock(Results* results, QWidget* parent = nullptr);
  void refreshKeys();  // repopulate the signal list from results

 private slots:
  void updatePlot();

 private:
  Results* results_;
  QListWidget* list_;
  TrendWidget* plot_;
};

}  // namespace umpnap

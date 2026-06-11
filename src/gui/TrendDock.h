#pragma once
#include <QDockWidget>
#include <map>
#include <string>
#include <utility>

class QListWidget;
class QListWidgetItem;

namespace umpnap {

class Results;
class TrendWidget;

// Signal picker + trend plot. Reads from a Results owned elsewhere. Supports a
// configurable time window, per-signal Y-range, hover readout, and PNG export.
class TrendDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit TrendDock(Results* results, QWidget* parent = nullptr);
  // Retarget at a different document's Results (multi-document tabs).
  void setResults(Results* results);
  void refreshKeys();   // repopulate the signal list from results
  void liveUpdate();    // during a run: populate once, then just replot

 private slots:
  void updatePlot();
  void onDoubleClicked(QListWidgetItem* item);  // set manual Y range
  void exportImage();

 private:
  Results* results_;
  QListWidget* list_;
  TrendWidget* plot_;
  std::map<std::string, std::pair<double, double>> ranges_;  // manual Y ranges
};

}  // namespace umpnap

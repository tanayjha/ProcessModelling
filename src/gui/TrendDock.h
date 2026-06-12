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
class Network;

// Signal picker + trend plot. Reads from a Results owned elsewhere. Supports a
// configurable time window, per-signal Y-range, hover readout, and PNG export.
class TrendDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit TrendDock(Results* results, QWidget* parent = nullptr);
  // Retarget at a different document's Results (multi-document tabs).
  void setResults(Results* results);
  // Network whose tags label the signals (so plots read by P&ID tag, not node
  // index / internal id). Should track the active document.
  void setNetwork(const Network* net) { net_ = net; }
  void refreshKeys();   // repopulate the signal list from results
  void liveUpdate();    // during a run: populate once, then just replot
  // Persist the currently-plotted signal selection into the network ("Configure
  // Graph" save) so the same signals re-plot when the project is reopened.
  void saveConfigTo(Network* net) const;

 private slots:
  void updatePlot();
  void onDoubleClicked(QListWidgetItem* item);  // set manual Y range
  void exportImage();

 private:
  // Tag-prefixed display label for a raw signal key (e.g. "comp.4.flow" ->
  // "P-101.flow", "node.2.pressure" -> "TK-1+P-101.pressure").
  QString prettyLabel(const std::string& rawKey) const;
  // Raw signal key carried on each list item (display text is the pretty label).
  static std::string itemKey(const QListWidgetItem* it);

  Results* results_;
  const Network* net_ = nullptr;
  QListWidget* list_;
  TrendWidget* plot_;
  std::map<std::string, std::pair<double, double>> ranges_;  // manual Y ranges
};

}  // namespace umpnap

#pragma once
#include <QColor>
#include <QString>
#include <QWidget>
#include <utility>
#include <vector>

namespace umpnap {

// Lightweight QPainter trend plot: multiple series, each auto- or manually
// scaled on its own Y range, a configurable time window, and a hover readout.
class TrendWidget : public QWidget {
  Q_OBJECT
 public:
  struct Series {
    QString label;
    QColor color;
    std::vector<std::pair<double, double>> data;  // (time, value)
    bool hasRange = false;   // manual Y range override
    double ymin = 0.0, ymax = 1.0;
  };

  explicit TrendWidget(QWidget* parent = nullptr);
  void setSeries(std::vector<Series> series);
  void setTimeWindow(double seconds);  // 0 = show all history
  double timeWindow() const { return window_; }

 protected:
  void paintEvent(QPaintEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void leaveEvent(QEvent* e) override;

 private:
  std::vector<Series> series_;
  double window_ = 0.0;
  bool hover_ = false;
  double hoverX_ = 0.0;  // widget x of the hover cursor
};

}  // namespace umpnap

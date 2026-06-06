#pragma once
#include <QColor>
#include <QString>
#include <QWidget>
#include <utility>
#include <vector>

namespace umpnap {

// Lightweight QPainter trend plot: multiple auto-scaled series + legend.
class TrendWidget : public QWidget {
  Q_OBJECT
 public:
  struct Series {
    QString label;
    QColor color;
    std::vector<std::pair<double, double>> data;  // (time, value)
  };

  explicit TrendWidget(QWidget* parent = nullptr);
  void setSeries(std::vector<Series> series);

 protected:
  void paintEvent(QPaintEvent* e) override;

 private:
  std::vector<Series> series_;
};

}  // namespace umpnap

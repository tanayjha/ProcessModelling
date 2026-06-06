#include "gui/TrendWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>

namespace umpnap {

TrendWidget::TrendWidget(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(180);
  setAutoFillBackground(true);
}

void TrendWidget::setSeries(std::vector<Series> series) {
  series_ = std::move(series);
  update();
}

void TrendWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(252, 252, 252));

  const int left = 44, right = 16, top = 14, bottom = 28;
  QRectF plot(left, top, width() - left - right, height() - top - bottom);
  p.setPen(QColor(180, 180, 180));
  p.drawRect(plot);

  // Global time range (shared X axis).
  bool any = false;
  double tmin = 0, tmax = 1;
  for (const auto& s : series_)
    for (const auto& pt : s.data) {
      if (!any) { tmin = tmax = pt.first; any = true; }
      else { tmin = std::min(tmin, pt.first); tmax = std::max(tmax, pt.first); }
    }
  if (!any) {
    p.setPen(Qt::gray);
    p.drawText(plot, Qt::AlignCenter, "No data - run a solve and select signals.");
    return;
  }
  if (tmax - tmin < 1e-12) tmax = tmin + 1.0;

  auto xMap = [&](double t) {
    return plot.left() + (t - tmin) / (tmax - tmin) * plot.width();
  };

  // Each series is normalised to its OWN range, so signals with very different
  // magnitudes (pressure ~1e5 vs level ~5 vs flow ~0.5) all show their shape.
  // The real min->max is shown in the legend so values are not lost.
  p.setPen(QColor(80, 80, 80));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignLeft, "t=" + QString::number(tmin, 'g', 3));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignRight, "t=" + QString::number(tmax, 'g', 3));

  int legendY = top + 4;
  for (const auto& s : series_) {
    if (s.data.empty()) continue;
    double lo = s.data.front().second, hi = s.data.front().second;
    for (const auto& pt : s.data) {
      lo = std::min(lo, pt.second);
      hi = std::max(hi, pt.second);
    }
    double span = hi - lo;
    auto yMap = [&](double v) {
      double frac = span > 1e-12 ? (v - lo) / span : 0.5;  // flat -> mid-height
      // 8% top/bottom padding so flat/edge lines stay inside the box.
      return plot.bottom() - (0.08 + 0.84 * frac) * plot.height();
    };

    QPen pen(s.color);
    pen.setWidth(2);
    p.setPen(pen);
    if (s.data.size() == 1) {
      p.setBrush(s.color);
      p.drawEllipse(QPointF(xMap(s.data[0].first), yMap(s.data[0].second)), 4, 4);
    } else {
      QPolygonF poly;
      for (const auto& pt : s.data) poly << QPointF(xMap(pt.first), yMap(pt.second));
      p.drawPolyline(poly);
    }

    // Legend: colour swatch + label + real value range.
    p.fillRect(QRectF(plot.right() - 220, legendY, 12, 4), s.color);
    p.setPen(QColor(50, 50, 50));
    QString rng = span > 1e-12
                      ? QString(" [%1 → %2]")
                            .arg(s.data.front().second, 0, 'g', 4)
                            .arg(s.data.back().second, 0, 'g', 4)
                      : QString(" [%1]").arg(lo, 0, 'g', 4);
    p.drawText(QRectF(plot.right() - 204, legendY - 6, 204, 16),
               Qt::AlignLeft, s.label + rng);
    legendY += 16;
  }
}

}  // namespace umpnap

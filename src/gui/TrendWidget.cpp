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

  const int left = 60, right = 16, top = 14, bottom = 28;
  QRectF plot(left, top, width() - left - right, height() - top - bottom);
  p.setPen(QColor(180, 180, 180));
  p.drawRect(plot);

  // Collect global ranges.
  bool any = false;
  double tmin = 0, tmax = 1, vmin = 0, vmax = 1;
  for (const auto& s : series_) {
    for (const auto& pt : s.data) {
      if (!any) {
        tmin = tmax = pt.first;
        vmin = vmax = pt.second;
        any = true;
      } else {
        tmin = std::min(tmin, pt.first);
        tmax = std::max(tmax, pt.first);
        vmin = std::min(vmin, pt.second);
        vmax = std::max(vmax, pt.second);
      }
    }
  }
  if (!any) {
    p.setPen(Qt::gray);
    p.drawText(plot, Qt::AlignCenter, "No data - run a solve and select signals.");
    return;
  }
  if (tmax - tmin < 1e-12) { tmax = tmin + 1.0; }
  if (vmax - vmin < 1e-12) { vmax = vmin + std::fabs(vmin) * 0.1 + 1.0; vmin -= 1.0; }
  double vpad = (vmax - vmin) * 0.05;
  vmin -= vpad;
  vmax += vpad;

  auto xMap = [&](double t) {
    return plot.left() + (t - tmin) / (tmax - tmin) * plot.width();
  };
  auto yMap = [&](double v) {
    return plot.bottom() - (v - vmin) / (vmax - vmin) * plot.height();
  };

  // Axis labels.
  p.setPen(QColor(80, 80, 80));
  p.drawText(QRectF(0, plot.top() - 2, left - 4, 16), Qt::AlignRight,
             QString::number(vmax, 'g', 4));
  p.drawText(QRectF(0, plot.bottom() - 14, left - 4, 16), Qt::AlignRight,
             QString::number(vmin, 'g', 4));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignLeft, "t=" + QString::number(tmin, 'g', 3));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignRight, "t=" + QString::number(tmax, 'g', 3));

  // Series + legend.
  int legendY = top + 4;
  for (const auto& s : series_) {
    QPen pen(s.color);
    pen.setWidth(2);
    p.setPen(pen);
    if (s.data.size() == 1) {
      QPointF c(xMap(s.data[0].first), yMap(s.data[0].second));
      p.setBrush(s.color);
      p.drawEllipse(c, 4, 4);
    } else {
      QPolygonF poly;
      for (const auto& pt : s.data) poly << QPointF(xMap(pt.first), yMap(pt.second));
      p.drawPolyline(poly);
    }
    // legend entry
    p.fillRect(QRectF(plot.right() - 130, legendY, 12, 4), s.color);
    p.setPen(QColor(50, 50, 50));
    p.drawText(QRectF(plot.right() - 114, legendY - 6, 120, 16),
               Qt::AlignLeft, s.label);
    legendY += 16;
  }
}

}  // namespace umpnap

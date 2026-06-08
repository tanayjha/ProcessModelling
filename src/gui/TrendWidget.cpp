#include "gui/TrendWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>

namespace umpnap {

TrendWidget::TrendWidget(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(180);
  setAutoFillBackground(true);
  setMouseTracking(true);  // for the hover readout
}

void TrendWidget::setSeries(std::vector<Series> series) {
  series_ = std::move(series);
  update();
}

void TrendWidget::setTimeWindow(double seconds) {
  window_ = seconds < 0 ? 0 : seconds;
  update();
}

void TrendWidget::mouseMoveEvent(QMouseEvent* e) {
  hover_ = true;
  hoverX_ = e->position().x();
  update();
}

void TrendWidget::leaveEvent(QEvent*) {
  hover_ = false;
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

  // Global time range, optionally clamped to the trailing window.
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
  if (window_ > 0 && tmax - window_ > tmin) tmin = tmax - window_;
  if (tmax - tmin < 1e-12) tmax = tmin + 1.0;

  auto xMap = [&](double t) {
    return plot.left() + (t - tmin) / (tmax - tmin) * plot.width();
  };

  p.setPen(QColor(80, 80, 80));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignLeft, "t=" + QString::number(tmin, 'g', 4));
  p.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18),
             Qt::AlignRight, "t=" + QString::number(tmax, 'g', 4));

  // Per-series Y scaling (manual range if set, else data min/max in window).
  int legendY = top + 4;
  double hoverT = (hover_) ? tmin + (hoverX_ - plot.left()) / plot.width() *
                                        (tmax - tmin)
                           : 0.0;
  std::vector<QString> hoverLabels;

  for (const auto& s : series_) {
    if (s.data.empty()) continue;
    double lo, hi;
    if (s.hasRange) {
      lo = s.ymin; hi = s.ymax;
    } else {
      lo = hi = s.data.front().second;
      for (const auto& pt : s.data) {
        if (window_ > 0 && pt.first < tmin) continue;
        lo = std::min(lo, pt.second);
        hi = std::max(hi, pt.second);
      }
    }
    double span = hi - lo;
    auto yMap = [&](double v) {
      double frac = span > 1e-12 ? (v - lo) / span : 0.5;
      frac = std::clamp(frac, 0.0, 1.0);
      return plot.bottom() - (0.08 + 0.84 * frac) * plot.height();
    };

    QPen pen(s.color);
    pen.setWidth(2);
    p.setPen(pen);
    QPolygonF poly;
    double hoverVal = 0.0;
    bool haveHover = false;
    for (const auto& pt : s.data) {
      if (window_ > 0 && pt.first < tmin) continue;
      poly << QPointF(xMap(pt.first), yMap(pt.second));
      if (hover_ && pt.first <= hoverT + 1e-9) { hoverVal = pt.second; haveHover = true; }
    }
    if (poly.size() == 1)
      p.drawEllipse(poly.front(), 4, 4);
    else
      p.drawPolyline(poly);

    // Legend with the (possibly manual) range.
    p.fillRect(QRectF(plot.right() - 230, legendY, 12, 4), s.color);
    p.setPen(QColor(50, 50, 50));
    p.drawText(QRectF(plot.right() - 214, legendY - 6, 214, 16), Qt::AlignLeft,
               s.label + QString(" [%1 … %2]").arg(lo, 0, 'g', 4).arg(hi, 0, 'g', 4));
    legendY += 16;

    if (haveHover)
      hoverLabels.push_back(
          QString("%1 = %2").arg(s.label).arg(hoverVal, 0, 'g', 5));
  }

  // Hover cursor + readout box.
  if (hover_ && hoverX_ >= plot.left() && hoverX_ <= plot.right()) {
    p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
    p.drawLine(QPointF(hoverX_, plot.top()), QPointF(hoverX_, plot.bottom()));
    hoverLabels.insert(hoverLabels.begin(),
                       QString("t = %1").arg(hoverT, 0, 'g', 5));
    int boxW = 150, boxH = 16 * (int)hoverLabels.size() + 8;
    double bx = hoverX_ + 10;
    if (bx + boxW > plot.right()) bx = hoverX_ - boxW - 10;
    QRectF box(bx, plot.top() + 6, boxW, boxH);
    p.setBrush(QColor(255, 255, 255, 235));
    p.setPen(QColor(150, 150, 150));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(QColor(30, 30, 30));
    for (int i = 0; i < (int)hoverLabels.size(); ++i)
      p.drawText(QRectF(box.left() + 6, box.top() + 4 + i * 16, boxW - 12, 16),
                 Qt::AlignLeft, hoverLabels[i]);
  }
}

}  // namespace umpnap

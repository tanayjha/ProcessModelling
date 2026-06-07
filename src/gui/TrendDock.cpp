#include "gui/TrendDock.h"

#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "core/Results.h"
#include "gui/TrendWidget.h"

namespace umpnap {

namespace {
const QColor kPalette[] = {
    QColor(31, 119, 180),  QColor(214, 39, 40),  QColor(44, 160, 44),
    QColor(148, 103, 189), QColor(255, 127, 14), QColor(140, 86, 75),
    QColor(23, 190, 207),  QColor(227, 119, 194)};
}  // namespace

TrendDock::TrendDock(Results* results, QWidget* parent)
    : QDockWidget("Trends", parent), results_(results) {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  list_ = new QListWidget(container);
  list_->setSelectionMode(QAbstractItemView::MultiSelection);
  list_->setMaximumHeight(140);
  plot_ = new TrendWidget(container);
  layout->addWidget(list_);
  layout->addWidget(plot_, 1);
  setWidget(container);
  connect(list_, &QListWidget::itemSelectionChanged, this, &TrendDock::updatePlot);
}

void TrendDock::refreshKeys() {
  list_->clear();
  for (const auto& k : results_->keys())
    list_->addItem(QString::fromStdString(k));

  // Auto-select signals that actually change, so a curve appears immediately
  // after a run without the user having to know to click. Capped to avoid
  // clutter; the user can adjust the selection afterwards.
  int selected = 0;
  for (int i = 0; i < list_->count() && selected < 4; ++i) {
    auto* item = list_->item(i);
    const auto* s = results_->series(item->text().toStdString());
    if (!s || s->size() < 2) continue;
    double lo = s->front().second, hi = s->front().second;
    for (const auto& tv : *s) {
      lo = std::min(lo, tv.second);
      hi = std::max(hi, tv.second);
    }
    if (hi - lo > 1e-9 * (1.0 + std::abs(hi))) {  // non-constant
      item->setSelected(true);
      ++selected;
    }
  }
  // If nothing varied (e.g. a steady run), show the first few signals anyway.
  if (selected == 0)
    for (int i = 0; i < list_->count() && i < 3; ++i)
      list_->item(i)->setSelected(true);

  updatePlot();
}

void TrendDock::liveUpdate() {
  // First data: populate the list and auto-select. Afterwards just replot so the
  // user's signal selection is preserved as the trends extend in time.
  if (list_->count() != (int)results_->keys().size())
    refreshKeys();
  else
    updatePlot();
}

void TrendDock::updatePlot() {
  std::vector<TrendWidget::Series> series;
  int ci = 0;
  for (auto* item : list_->selectedItems()) {
    std::string key = item->text().toStdString();
    const auto* data = results_->series(key);
    if (!data) continue;
    TrendWidget::Series s;
    s.label = item->text();
    s.color = kPalette[ci % 8];
    s.data = *data;
    series.push_back(std::move(s));
    ++ci;
  }
  plot_->setSeries(std::move(series));
}

}  // namespace umpnap

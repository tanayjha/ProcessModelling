#include "gui/TrendDock.h"

#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>

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

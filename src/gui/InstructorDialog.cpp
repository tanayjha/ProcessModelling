#include "gui/InstructorDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <string>
#include <vector>

#include "core/Network.h"

namespace umpnap {

namespace {
// Display label -> stored malfunction code (see solver/Malfunctions).
const std::vector<std::pair<QString, QString>> kMalf = {
    {"None", ""},        {"Fail open", "failOpen"}, {"Fail close", "failClose"},
    {"Stuck 50%", "stuck"}, {"Trip", "trip"},       {"Breaker open", "open"}};
const std::vector<std::pair<QString, QString>> kOverride = {
    {"None", ""}, {"Force open", "open"}, {"Force close", "close"}};

int indexOfCode(const std::vector<std::pair<QString, QString>>& v,
                const std::string& code) {
  for (int i = 0; i < (int)v.size(); ++i)
    if (v[i].second.toStdString() == code) return i;
  return 0;
}
}  // namespace

InstructorDialog::InstructorDialog(Network* net, QWidget* parent)
    : QDialog(parent), net_(net) {
  setWindowTitle("Instructor station — malfunctions & overrides");
  resize(560, 420);
  auto* lay = new QVBoxLayout(this);
  table_ = new QTableWidget(this);
  table_->setColumnCount(4);
  table_->setHorizontalHeaderLabels({"Tag", "Type", "Malfunction", "Local override"});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  lay->addWidget(table_);
  rebuild();
}

void InstructorDialog::rebuild() {
  const auto& comps = net_->components();
  table_->setRowCount((int)comps.size());
  int row = 0;
  for (const auto& uc : comps) {
    Component* c = uc.get();
    table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(c->name)));
    table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(c->type)));

    auto* malf = new QComboBox(table_);
    for (const auto& kv : kMalf) malf->addItem(kv.first);
    malf->setCurrentIndex(indexOfCode(kMalf, c->cfg("malf")));
    connect(malf, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, c](int i) {
              c->config["malf"] = kMalf[i].second.toStdString();
              if (kMalf[i].second == "stuck") c->config["malfVal"] = "0.5";
              emit changed();
            });
    table_->setCellWidget(row, 2, malf);

    auto* ov = new QComboBox(table_);
    for (const auto& kv : kOverride) ov->addItem(kv.first);
    ov->setCurrentIndex(indexOfCode(kOverride, c->cfg("override")));
    connect(ov, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, c](int i) {
              c->config["override"] = kOverride[i].second.toStdString();
              emit changed();
            });
    table_->setCellWidget(row, 3, ov);
    ++row;
  }
  table_->resizeColumnsToContents();
}

}  // namespace umpnap

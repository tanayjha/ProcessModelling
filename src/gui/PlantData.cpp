#include "gui/PlantData.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"
#include "core/Network.h"
#include "gui/EnumParams.h"

namespace umpnap {

// ============================== Curve editor ===============================

CurveEditorDialog::CurveEditorDialog(Component* c, const QString& curveKey,
                                     const QString& xLabel, const QString& yLabel,
                                     QWidget* parent)
    : QDialog(parent), comp_(c), key_(curveKey) {
  setWindowTitle("Edit " + curveKey + " curve  -  " +
                 QString::fromStdString(c->name));
  resize(360, 320);
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(
      "Enter measured curve points. ≥ 3 points are fitted to a quadratic;\n"
      "fewer falls back to the datasheet rated/shutoff model."));

  table_ = new QTableWidget(0, 2, this);
  table_->setHorizontalHeaderLabels({xLabel, yLabel});
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(table_);

  // Seed from existing curve, or from the pump's rated/shutoff scalars.
  const auto* existing = comp_->curve(key_.toStdString());
  std::vector<std::pair<double, double>> seed;
  if (existing && !existing->empty()) {
    seed = *existing;
  } else if (comp_->type == "Pump") {
    double Qr = comp_->param("ratedFlow"), Hr = comp_->param("ratedHead"),
           H0 = comp_->param("shutoffHead");
    seed = {{0.0, H0}, {Qr, Hr}, {1.5 * Qr, Hr - 0.6 * (H0 - Hr)}};
  }
  for (const auto& pt : seed) {
    int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(QString::number(pt.first, 'g', 6)));
    table_->setItem(row, 1, new QTableWidgetItem(QString::number(pt.second, 'g', 6)));
  }

  auto* btns = new QHBoxLayout();
  auto* add = new QPushButton("Add Row", this);
  auto* rem = new QPushButton("Remove Row", this);
  btns->addWidget(add);
  btns->addWidget(rem);
  btns->addStretch();
  layout->addLayout(btns);
  connect(add, &QPushButton::clicked, this, &CurveEditorDialog::addRow);
  connect(rem, &QPushButton::clicked, this, &CurveEditorDialog::removeRow);

  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                   this);
  connect(box, &QDialogButtonBox::accepted, this, &CurveEditorDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(box);
}

void CurveEditorDialog::addRow() {
  int row = table_->rowCount();
  table_->insertRow(row);
  table_->setItem(row, 0, new QTableWidgetItem("0"));
  table_->setItem(row, 1, new QTableWidgetItem("0"));
}

void CurveEditorDialog::removeRow() {
  int row = table_->currentRow();
  if (row < 0) row = table_->rowCount() - 1;
  if (row >= 0) table_->removeRow(row);
}

void CurveEditorDialog::accept() {
  std::vector<std::pair<double, double>> pts;
  for (int r = 0; r < table_->rowCount(); ++r) {
    auto* xi = table_->item(r, 0);
    auto* yi = table_->item(r, 1);
    if (!xi || !yi) continue;
    bool okx = false, oky = false;
    double x = xi->text().toDouble(&okx);
    double y = yi->text().toDouble(&oky);
    if (okx && oky) pts.push_back({x, y});
  }
  comp_->curves[key_.toStdString()] = std::move(pts);
  QDialog::accept();
}

// ============================ Plant datasheet ==============================

PlantDataDialog::PlantDataDialog(Network* net, QWidget* parent)
    : QDialog(parent), net_(net) {
  setWindowTitle("Plant Data");
  resize(900, 480);
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(
      "Edit design data for every tagged component. Changes apply immediately; "
      "re-run the solver to see the effect on the response."));
  tabs_ = new QTabWidget(this);
  layout->addWidget(tabs_);

  // One tab per registered type that has at least one instance, in library order.
  for (const auto& def : ComponentRegistry::instance().defs()) {
    bool present = false;
    for (const auto& c : net_->components())
      if (c->type == def.type) { present = true; break; }
    if (present) buildTabFor(def.type);
  }

  auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget(box);
}

void PlantDataDialog::buildTabFor(const std::string& type) {
  const ComponentDef* def = ComponentRegistry::instance().find(type);
  if (!def) return;
  bool hydraulic = def->domain == Domain::Hydraulic;
  bool isPump = type == "Pump";

  // Gather instances of this type.
  std::vector<Component*> comps;
  for (const auto& c : net_->components())
    if (c->type == type) comps.push_back(c.get());

  // Columns: Tag, [Fluid], params..., [Head Curve].
  QStringList headers;
  headers << "Tag";
  if (hydraulic) headers << "Fluid";
  for (const auto& ps : def->params) {
    QString h = QString::fromStdString(ps.name);
    if (!ps.unit.empty() && ps.unit != "-") h += " [" + QString::fromStdString(ps.unit) + "]";
    headers << h;
  }
  if (isPump) headers << "Head Curve";

  auto* table = new QTableWidget((int)comps.size(), headers.size(), this);
  table->setHorizontalHeaderLabels(headers);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table->blockSignals(true);

  for (int row = 0; row < (int)comps.size(); ++row) {
    Component* c = comps[row];
    int col = 0;

    // Tag (stores the component id for write-back).
    auto* tag = new QTableWidgetItem(QString::fromStdString(c->name));
    tag->setData(Qt::UserRole, c->id);
    table->setItem(row, col++, tag);

    // Fluid (combo).
    if (hydraulic) {
      auto* combo = new QComboBox(table);
      for (const auto& n : FluidLibrary::names())
        combo->addItem(QString::fromStdString(n));
      combo->setCurrentText(QString::fromStdString(c->fluid));
      Component* cap = c;
      connect(combo, &QComboBox::currentTextChanged, this,
              [this, cap](const QString& t) {
                cap->fluid = t.toStdString();
                emit dataChanged();
              });
      table->setCellWidget(row, col++, combo);
    }

    // Parameters (editable numeric text; header label == param name).
    for (const auto& ps : def->params) {
      if (EnumSpec es = enumOptions(ps.name); !es.options.isEmpty()) {
        auto* combo = new QComboBox(table);
        combo->addItems(es.options);
        int idx = (int)c->param(ps.name);
        combo->setCurrentIndex(idx < 0 || idx >= es.options.size() ? 0 : idx);
        Component* cap = c;
        std::string key = ps.name;
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, cap, key](int i) {
                  cap->params[key] = i;
                  emit dataChanged();
                });
        table->setCellWidget(row, col++, combo);
        continue;
      }
      auto* item =
          new QTableWidgetItem(QString::number(c->param(ps.name), 'g', 6));
      table->setItem(row, col++, item);
    }

    // Pump head-curve editor button.
    if (isPump) {
      auto* btn = new QPushButton("Edit…", table);
      Component* cap = c;
      connect(btn, &QPushButton::clicked, this, [this, cap]() {
        CurveEditorDialog dlg(cap, "head", "Q [m3/s]", "H [m]", this);
        if (dlg.exec() == QDialog::Accepted) emit dataChanged();
      });
      table->setCellWidget(row, col++, btn);
    }
  }
  table->blockSignals(false);

  // Write-back on edit. Column 0 is the tag; other text columns are params
  // identified by stripping the unit suffix from the header label.
  connect(table, &QTableWidget::itemChanged, this,
          [this, table, def](QTableWidgetItem* item) {
            auto* tagItem = table->item(item->row(), 0);
            if (!tagItem) return;
            int id = tagItem->data(Qt::UserRole).toInt();
            Component* c = net_->component(id);
            if (!c) return;
            if (item->column() == 0) {
              c->name = item->text().toStdString();
              emit dataChanged();
              return;
            }
            QString header = table->horizontalHeaderItem(item->column())->text();
            QString paramName = header.section(" [", 0, 0);
            bool ok = false;
            double v = item->text().toDouble(&ok);
            if (ok) {
              c->params[paramName.toStdString()] = v;
              emit dataChanged();
            }
          });

  tabs_->addTab(table, QString::fromStdString(type) + "  (" +
                           QString::number(comps.size()) + ")");
}

}  // namespace umpnap

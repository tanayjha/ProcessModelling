#include "gui/PropertyEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"
#include "core/Network.h"
#include "gui/Equations.h"
#include "gui/PlantData.h"

namespace umpnap {

PropertyEditor::PropertyEditor(QWidget* parent)
    : QDockWidget("Properties", parent) {
  rebuild();
}

void PropertyEditor::showComponent(Component* c) {
  comp_ = c;
  rebuild();
}

void PropertyEditor::rebuild() {
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  body_ = new QWidget(scroll);
  auto* form = new QFormLayout(body_);

  if (!comp_) {
    form->addRow(new QLabel("Select a component to edit its parameters."));
    scroll->setWidget(body_);
    setWidget(scroll);
    return;
  }

  form->addRow(new QLabel("<b>" + QString::fromStdString(comp_->type) + " #" +
                          QString::number(comp_->id) + "</b>"));

  // Editable P&ID tag.
  auto* tagEdit = new QLineEdit(QString::fromStdString(comp_->name), body_);
  connect(tagEdit, &QLineEdit::editingFinished, this, [this, tagEdit]() {
    if (comp_) comp_->name = tagEdit->text().toStdString();
    emit edited();
  });
  form->addRow("Tag", tagEdit);

  // Fluid selector (generic property framework).
  const ComponentDef* def = ComponentRegistry::instance().find(comp_->type);

  // Fluid selector (only meaningful for hydraulic-domain components).
  if (def && def->domain == Domain::Hydraulic) {
    auto* fluidBox = new QComboBox(body_);
    for (const auto& n : FluidLibrary::names())
      fluidBox->addItem(QString::fromStdString(n));
    fluidBox->setCurrentText(QString::fromStdString(comp_->fluid));
    connect(fluidBox, &QComboBox::currentTextChanged, this,
            [this](const QString& t) {
              if (comp_) comp_->fluid = t.toStdString();
              emit edited();
            });
    form->addRow("Fluid", fluidBox);
  }

  // Parameters from the registry schema (with units).
  if (def) {
    for (const auto& ps : def->params) {
      auto* edit = new QLineEdit(body_);
      edit->setText(QString::number(comp_->param(ps.name)));
      std::string key = ps.name;
      connect(edit, &QLineEdit::editingFinished, this, [this, edit, key]() {
        bool ok = false;
        double v = edit->text().toDouble(&ok);
        if (ok && comp_) {
          comp_->params[key] = v;
          emit edited();
        }
      });
      QString label = QString::fromStdString(ps.name);
      if (!ps.unit.empty() && ps.unit != "-")
        label += "  [" + QString::fromStdString(ps.unit) + "]";
      form->addRow(label, edit);
    }
  }

  // Pump head-flow curve editor.
  if (comp_->type == "Pump") {
    auto* curveBtn = new QPushButton("Edit Head Curve…", body_);
    connect(curveBtn, &QPushButton::clicked, this, [this]() {
      if (!comp_) return;
      CurveEditorDialog dlg(comp_, "head", "Q [m3/s]", "H [m]", this);
      if (dlg.exec() == QDialog::Accepted) emit edited();
    });
    form->addRow("Head curve", curveBtn);
  }

  // Controller links (PID loop wiring by tag).
  if (comp_->type == "Controller") {
    auto addCfg = [&](const QString& label, const std::string& key,
                      const QString& placeholder) {
      auto* edit = new QLineEdit(
          QString::fromStdString(comp_->cfg(key)), body_);
      edit->setPlaceholderText(placeholder);
      connect(edit, &QLineEdit::editingFinished, this, [this, edit, key]() {
        if (comp_) comp_->config[key] = edit->text().toStdString();
        emit edited();
      });
      form->addRow(label, edit);
    };
    addCfg("Measured tag", "measComp", "e.g. TK-1");
    auto* varBox = new QComboBox(body_);
    varBox->addItems({"level", "flow", "pressure"});
    QString cur = QString::fromStdString(comp_->cfg("measVar"));
    if (!cur.isEmpty()) varBox->setCurrentText(cur);
    connect(varBox, &QComboBox::currentTextChanged, this, [this](const QString& t) {
      if (comp_) comp_->config["measVar"] = t.toStdString();
      emit edited();
    });
    form->addRow("Measured var", varBox);
    addCfg("Output valve tag", "output", "e.g. FCV-1");
  }

  // Governing equations for this library model.
  QString eq = equationText(comp_->type);
  if (!eq.isEmpty()) {
    auto* divider = new QFrame(body_);
    divider->setFrameShape(QFrame::HLine);
    form->addRow(divider);
    form->addRow(new QLabel("<b>Governing equations</b>"));
    auto* eqLabel = new QLabel(eq, body_);
    eqLabel->setWordWrap(true);
    eqLabel->setStyleSheet("font-family: monospace; color: #333;");
    eqLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(eqLabel);
  }

  scroll->setWidget(body_);
  setWidget(scroll);
}

}  // namespace umpnap

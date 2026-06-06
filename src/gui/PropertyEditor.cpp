#include "gui/PropertyEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QWidget>

#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"
#include "core/Network.h"

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

  // Fluid selector (generic property framework).
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

  // Parameters from the registry schema (with units).
  const ComponentDef* def = ComponentRegistry::instance().find(comp_->type);
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

  scroll->setWidget(body_);
  setWidget(scroll);
}

}  // namespace umpnap

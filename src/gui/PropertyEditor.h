#pragma once
#include <QDockWidget>
#include <QString>

class QWidget;

namespace umpnap {

class Component;

// Reflects the selected component's parameter schema and fluid choice.
// Edits are written straight back into the Component.
class PropertyEditor : public QDockWidget {
  Q_OBJECT
 public:
  explicit PropertyEditor(QWidget* parent = nullptr);

 public slots:
  void showComponent(umpnap::Component* c);  // nullptr clears the editor

 signals:
  void edited();  // a parameter or fluid changed

 private:
  void rebuild();
  Component* comp_ = nullptr;
  QWidget* body_ = nullptr;
};

}  // namespace umpnap

#pragma once
#include <QDialog>

class QTabWidget;
class QTableWidget;

namespace umpnap {

class Network;
class Component;

// Edits a component's tabular curve (e.g. a pump "head" curve of (Q,H) points).
class CurveEditorDialog : public QDialog {
  Q_OBJECT
 public:
  CurveEditorDialog(Component* c, const QString& curveKey, const QString& xLabel,
                    const QString& yLabel, QWidget* parent = nullptr);

 private slots:
  void addRow();
  void removeRow();
  void accept() override;

 private:
  Component* comp_;
  QString key_;
  QTableWidget* table_;
};

// "Plant Data" datasheet: one tab per component type, a table of the tagged
// instances (rows) against their design fields (columns). Edits write straight
// back into the model so the next solve reflects them.
class PlantDataDialog : public QDialog {
  Q_OBJECT
 public:
  explicit PlantDataDialog(Network* net, QWidget* parent = nullptr);

 signals:
  void dataChanged();

 private:
  void buildTabFor(const std::string& type);
  Network* net_;
  QTabWidget* tabs_;
};

}  // namespace umpnap

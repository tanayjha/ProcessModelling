#pragma once
#include <QDialog>

class QTableWidget;

namespace umpnap {

class Network;

// Instructor station: initiate or clear malfunctions on any component and force
// local valve overrides, the way a training simulator's instructor desk does.
// Selections are written to each component's config ("malf"/"malfVal"/"override")
// and take effect on the next solve cycle (see solver/Malfunctions). Multiple
// components may each hold their own malfunction simultaneously.
class InstructorDialog : public QDialog {
  Q_OBJECT
 public:
  InstructorDialog(Network* net, QWidget* parent = nullptr);

 signals:
  void changed();  // a malfunction/override was applied -> caller should re-solve

 private:
  void rebuild();
  Network* net_ = nullptr;
  QTableWidget* table_ = nullptr;
};

}  // namespace umpnap

#pragma once
#include <QObject>
#include <QTimer>
#include <map>
#include <string>

#include "solver/SolverManager.h"

namespace umpnap {

class Network;
class Results;

// Interactive simulation engine: drives the solver one cycle per tick with
// Run / Pause(Freeze) / Single-Step / Fast-Forward / Reset, plus in-memory
// snapshots and a capturable Initial Condition. One tick advances exactly
// `speed` solver cycles; Single-Step advances exactly one (for debugging).
class SimController : public QObject {
  Q_OBJECT
 public:
  enum Mode { Stopped, Running, Paused };

  SimController(Network* net, Results* results, QObject* parent = nullptr);

  Mode mode() const { return mode_; }
  double time() const { return time_; }
  double dt() const { return dt_; }
  int speed() const { return speed_; }
  void setDt(double dt) { if (dt > 0) dt_ = dt; }
  void setSpeed(int factor) { speed_ = factor < 1 ? 1 : factor; }

 public slots:
  void initializeSteady();  // steady-state init at t=0; captures the IC
  void start();             // Run
  void pause();             // Freeze (stop time, allow inspection/edits)
  void singleStep();        // advance exactly one solver cycle
  void reset();             // restore Initial Condition, t=0
  void captureInitial();    // set current plant state as the Initial Condition
  void saveSnapshot();      // capture full state at the current sim time
  void restoreSnapshot();   // restore the last snapshot

 signals:
  void updated();      // results/state changed; refresh views
  void modeChanged();  // running/paused/stopped transition

 private slots:
  void onTimer();

 private:
  void doStep();  // one solver cycle, no signal
  std::map<int, std::map<std::string, double>> grabState() const;
  void applyState(const std::map<int, std::map<std::string, double>>& s);

  Network* net_;
  Results* results_;
  SolverManager solver_;
  QTimer timer_;
  Mode mode_ = Stopped;
  double dt_ = 1.0;
  double time_ = 0.0;
  int speed_ = 1;
  bool icCaptured_ = false;
  bool hasSnapshot_ = false;
  double snapTime_ = 0.0;
  std::map<int, std::map<std::string, double>> initial_;
  std::map<int, std::map<std::string, double>> snapshot_;
};

}  // namespace umpnap

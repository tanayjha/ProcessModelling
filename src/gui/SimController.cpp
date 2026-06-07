#include "gui/SimController.h"

#include "core/Network.h"
#include "core/Results.h"

namespace umpnap {

SimController::SimController(Network* net, Results* results, QObject* parent)
    : QObject(parent), net_(net), results_(results) {
  timer_.setInterval(120);  // wall-clock tick; `speed` cycles run per tick
  connect(&timer_, &QTimer::timeout, this, &SimController::onTimer);
}

std::map<int, std::map<std::string, double>> SimController::grabState() const {
  std::map<int, std::map<std::string, double>> s;
  for (const auto& c : net_->components()) s[c->id] = c->params;
  return s;
}

void SimController::applyState(
    const std::map<int, std::map<std::string, double>>& s) {
  for (const auto& kv : s) {
    Component* c = net_->component(kv.first);
    if (c) c->params = kv.second;
  }
}

void SimController::captureInitial() {
  initial_ = grabState();
  icCaptured_ = true;
}

void SimController::initializeSteady() {
  if (!icCaptured_) captureInitial();
  results_->clear();
  time_ = 0.0;
  solver_.runSteady(*net_, *results_);
  emit updated();
}

void SimController::doStep() {
  if (results_->keys().empty()) {
    if (!icCaptured_) captureInitial();
    solver_.runSteady(*net_, *results_);  // seed t=0
  }
  time_ += dt_;
  solver_.stepTransient(*net_, *results_, dt_, time_);
}

void SimController::start() {
  if (mode_ == Running) return;
  if (results_->keys().empty()) initializeSteady();
  mode_ = Running;
  timer_.start();
  emit modeChanged();
}

void SimController::pause() {
  if (mode_ != Running) return;
  mode_ = Paused;
  timer_.stop();
  emit modeChanged();
}

void SimController::singleStep() {
  if (mode_ == Running) {  // freeze first, then advance one cycle
    mode_ = Paused;
    timer_.stop();
  }
  doStep();
  if (mode_ == Stopped) mode_ = Paused;
  emit updated();
  emit modeChanged();
}

void SimController::reset() {
  timer_.stop();
  mode_ = Stopped;
  if (icCaptured_) applyState(initial_);
  time_ = 0.0;
  results_->clear();
  solver_.runSteady(*net_, *results_);
  emit updated();
  emit modeChanged();
}

void SimController::saveSnapshot() {
  snapshot_ = grabState();
  snapTime_ = time_;
  hasSnapshot_ = true;
}

void SimController::restoreSnapshot() {
  if (!hasSnapshot_) return;
  applyState(snapshot_);
  time_ = snapTime_;
  solver_.runSteady(*net_, *results_);  // re-establish balance at restored state
  emit updated();
}

void SimController::onTimer() {
  for (int i = 0; i < speed_; ++i) doStep();
  emit updated();
}

}  // namespace umpnap

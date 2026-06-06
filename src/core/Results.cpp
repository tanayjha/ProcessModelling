#include "core/Results.h"

#include <algorithm>
#include <cstdio>
#include <set>

namespace umpnap {

int Results::indexOf(const std::string& key) const {
  for (size_t i = 0; i < keys_.size(); ++i)
    if (keys_[i] == key) return (int)i;
  return -1;
}

void Results::record(const std::string& key, double t, double value) {
  int i = indexOf(key);
  if (i < 0) {
    keys_.push_back(key);
    data_.emplace_back();
    i = (int)keys_.size() - 1;
  }
  data_[i].push_back({t, value});
}

const std::vector<std::pair<double, double>>* Results::series(const std::string& key) const {
  int i = indexOf(key);
  return i < 0 ? nullptr : &data_[i];
}

double Results::latest(const std::string& key) const {
  int i = indexOf(key);
  if (i < 0 || data_[i].empty()) return 0.0;
  return data_[i].back().second;
}

std::vector<std::string> Results::keys() const { return keys_; }

void Results::clear() {
  keys_.clear();
  data_.clear();
}

bool Results::writeCsv(const std::string& path) const {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;

  // Union of all timestamps, sorted.
  std::set<double> times;
  for (const auto& series : data_)
    for (const auto& tv : series) times.insert(tv.first);

  std::fprintf(f, "time");
  for (const auto& k : keys_) std::fprintf(f, ",%s", k.c_str());
  std::fprintf(f, "\n");

  for (double t : times) {
    std::fprintf(f, "%g", t);
    for (const auto& series : data_) {
      // Nearest-at-or-before value for this timestamp.
      double v = 0.0;
      bool found = false;
      for (const auto& tv : series) {
        if (tv.first <= t + 1e-12) { v = tv.second; found = true; }
      }
      if (found)
        std::fprintf(f, ",%g", v);
      else
        std::fprintf(f, ",");
    }
    std::fprintf(f, "\n");
  }
  std::fclose(f);
  return true;
}

}  // namespace umpnap

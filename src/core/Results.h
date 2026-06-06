#pragma once
#include <string>
#include <utility>
#include <vector>

namespace umpnap {

// Time-series store. Keys: "node.<n>.pressure", "comp.<id>.flow",
// "comp.<id>.head", "comp.<id>.position", "comp.<id>.level", etc.
class Results {
 public:
  void record(const std::string& key, double t, double value);
  // Returns nullptr if the key has no data.
  const std::vector<std::pair<double, double>>* series(const std::string& key) const;
  // Latest value for a key (0 if absent).
  double latest(const std::string& key) const;
  std::vector<std::string> keys() const;
  void clear();
  bool writeCsv(const std::string& path) const;

 private:
  // Parallel vectors keep insertion order of keys stable for the GUI.
  std::vector<std::string> keys_;
  std::vector<std::vector<std::pair<double, double>>> data_;
  int indexOf(const std::string& key) const;
};

}  // namespace umpnap

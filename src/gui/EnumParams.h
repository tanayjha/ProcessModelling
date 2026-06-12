#pragma once

#include <QString>
#include <QStringList>

#include <string>

namespace umpnap {

// Enumerated ("dropdown") parameters. A parameter whose name appears here is
// rendered as a named QComboBox in both the property editor and the plant-data
// datasheet; its stored value is the selected option's index (0-based). Keeping
// the mapping in one place lets a new dropdown be added by a single entry here
// plus the matching ParamSpec in the registry (with unit/min/max bounding the
// index range). Returns an empty options list for non-enumerated parameters.
struct EnumSpec {
  QString label;        // row label shown in the editors
  QStringList options;  // option text, index-ordered
};

inline EnumSpec enumOptions(const std::string& name) {
  if (name == "characteristic")
    return {"Characteristic", {"Linear", "Equal-percentage", "Quick-opening"}};
  if (name == "shape")
    return {"Tank shape", {"Rectangular", "Cylindrical"}};
  if (name == "valveType")
    return {"Valve air action", {"Air-to-open", "Air-to-close", "Air-to-open/air-to-close"}};
  if (name == "actChar")
    return {"Characterization", {"Linear", "Square-root", "Equal-percentage", "Quick-opening"}};
  if (name == "action")
    return {"Action", {"Normal", "Reverse"}};
  if (name == "xchar")
    return {"Characteristic", {"Linear", "Square-root", "Square"}};
  return {};
}

}  // namespace umpnap

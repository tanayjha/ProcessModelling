#pragma once
#include <map>
#include <string>
#include <vector>

namespace umpnap {

enum class PortRole { Inlet, Outlet, Bidirectional };

struct Port {
  std::string name;
  PortRole role = PortRole::Bidirectional;
};

// Parameter schema entry (registry) and metadata for the property editor.
struct ParamSpec {
  std::string name;
  std::string unit;
  double def = 0.0;
  double min = 0.0;
  double max = 0.0;  // max==0 with min==0 means "unbounded" for the editor
};

enum class Domain { Hydraulic, Gas, Thermal, Electrical, Control, Instrument };

const char* domainName(Domain d);
Domain domainFromName(const std::string& s);

class Component {
 public:
  int id = -1;
  std::string type;          // library model type, e.g. "Pipe"
  std::string name;          // user P&ID tag, e.g. "P-101" (defaulted on add)
  Domain domain = Domain::Hydraulic;
  double x = 0.0, y = 0.0;  // canvas position
  std::string fluid = "Light Water";
  std::map<std::string, double> params;
  // Tabular curve data keyed by name, e.g. "head" -> [(Q,H), ...] for a pump.
  std::map<std::string, std::vector<std::pair<double, double>>> curves;
  std::vector<Port> ports;

  double param(const std::string& k) const {
    auto it = params.find(k);
    return it == params.end() ? 0.0 : it->second;
  }
  const std::vector<std::pair<double, double>>* curve(const std::string& k) const {
    auto it = curves.find(k);
    return it == curves.end() ? nullptr : &it->second;
  }
  bool hasPort(const std::string& p) const {
    for (const auto& pt : ports)
      if (pt.name == p) return true;
    return false;
  }
};

}  // namespace umpnap

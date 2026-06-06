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

enum class Domain { Hydraulic, Gas, Thermal, Electrical, Control };

const char* domainName(Domain d);
Domain domainFromName(const std::string& s);

class Component {
 public:
  int id = -1;
  std::string type;  // e.g. "Pipe"
  Domain domain = Domain::Hydraulic;
  double x = 0.0, y = 0.0;  // canvas position
  std::string fluid = "Light Water";
  std::map<std::string, double> params;
  std::vector<Port> ports;

  double param(const std::string& k) const {
    auto it = params.find(k);
    return it == params.end() ? 0.0 : it->second;
  }
  bool hasPort(const std::string& p) const {
    for (const auto& pt : ports)
      if (pt.name == p) return true;
    return false;
  }
};

}  // namespace umpnap

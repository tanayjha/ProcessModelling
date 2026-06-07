#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/Component.h"

namespace umpnap {

struct ComponentDef {
  std::string type;
  Domain domain = Domain::Hydraulic;
  std::string tagPrefix;  // default P&ID tag prefix, e.g. "P" -> "P-3"
  std::vector<Port> ports;
  std::vector<ParamSpec> params;
  std::string defaultFluid = "Light Water";  // working fluid for new instances
};

// Plugin-style registry. Adding a component type = register one ComponentDef.
class ComponentRegistry {
 public:
  static ComponentRegistry& instance();
  void registerDef(const ComponentDef& d);  // ignores duplicates
  // Creates a component with ports and parameter defaults filled in.
  std::unique_ptr<Component> create(const std::string& type) const;
  const std::vector<ComponentDef>& defs() const { return defs_; }
  const ComponentDef* find(const std::string& type) const;

 private:
  std::vector<ComponentDef> defs_;
};

// Registers the 8 Phase-1 hydraulic component types. Idempotent.
void registerHydraulicComponents();

}  // namespace umpnap

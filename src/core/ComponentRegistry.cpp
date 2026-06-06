#include "core/ComponentRegistry.h"

namespace umpnap {

ComponentRegistry& ComponentRegistry::instance() {
  static ComponentRegistry r;
  return r;
}

void ComponentRegistry::registerDef(const ComponentDef& d) {
  if (find(d.type)) return;
  defs_.push_back(d);
}

const ComponentDef* ComponentRegistry::find(const std::string& type) const {
  for (const auto& d : defs_)
    if (d.type == type) return &d;
  return nullptr;
}

std::unique_ptr<Component> ComponentRegistry::create(const std::string& type) const {
  const ComponentDef* d = find(type);
  if (!d) return nullptr;
  auto c = std::make_unique<Component>();
  c->type = d->type;
  c->domain = d->domain;
  c->ports = d->ports;
  for (const auto& ps : d->params) c->params[ps.name] = ps.def;
  return c;
}

void registerHydraulicComponents() {
  auto& reg = ComponentRegistry::instance();
  const PortRole IN = PortRole::Inlet;
  const PortRole OUT = PortRole::Outlet;
  const PortRole BI = PortRole::Bidirectional;
  const Domain H = Domain::Hydraulic;

  // {name, unit, default, min, max}
  reg.registerDef({"Boundary", H, {{"p", BI}},
                   {{"pressure", "Pa", 2.0e5, 0.0, 0.0}}});

  reg.registerDef({"Tank", H, {{"p", BI}},
                   {{"area", "m^2", 1.0, 0.0, 0.0},
                    {"level", "m", 2.0, 0.0, 0.0},
                    {"p_top", "Pa", 1.013e5, 0.0, 0.0}}});

  reg.registerDef({"Pipe", H, {{"in", IN}, {"out", OUT}},
                   {{"length", "m", 10.0, 0.0, 0.0},
                    {"diameter", "m", 0.1, 1e-3, 0.0},
                    {"roughness", "m", 4.5e-5, 0.0, 0.0}}});

  reg.registerDef({"Valve", H, {{"in", IN}, {"out", OUT}},
                   {{"Kv", "-", 5.0, 1e-3, 0.0},
                    {"position", "-", 1.0, 0.0, 1.0}}});

  reg.registerDef({"Orifice", H, {{"in", IN}, {"out", OUT}},
                   {{"bore", "m", 0.05, 1e-3, 0.0},
                    {"Cd", "-", 0.62, 0.1, 1.0}}});

  reg.registerDef({"Pump", H, {{"in", IN}, {"out", OUT}},
                   {{"H0", "m", 50.0, 0.0, 0.0},
                    {"a", "s^2/m^5", 5.0e3, 0.0, 0.0}}});

  reg.registerDef({"Junction", H, {{"a", BI}, {"b", BI}, {"c", BI}}, {}});

  reg.registerDef({"HeatExchanger", H, {{"in", IN}, {"out", OUT}},
                   {{"K_hx", "-", 2.0e4, 0.0, 0.0}}});
}

}  // namespace umpnap

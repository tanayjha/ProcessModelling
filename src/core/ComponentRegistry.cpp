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
  const Domain I = Domain::Instrument;
  const Domain C = Domain::Control;

  // Each ParamSpec is {name, unit, default, min, max}. Values are stored in SI.
  // ----------------------- Hydraulic (solving) ----------------------------

  // Pressure boundary / reservoir. Anchors the network pressure.
  reg.registerDef({"Boundary", H, "BND", {{"p", BI}},
                   {{"pressure", "Pa", 2.0e5, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Vertical cylindrical tank. Node pressure = p_top + rho*g*level.
  reg.registerDef({"Tank", H, "TK", {{"p", BI}},
                   {{"diameter", "m", 2.0, 1e-3, 0.0},
                    {"height", "m", 6.0, 0.0, 0.0},
                    {"level", "m", 2.0, 0.0, 0.0},
                    {"p_top", "Pa", 1.013e5, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Pipe: Darcy-Weisbach + minor losses. ID drives flow area; OD is wall info.
  reg.registerDef({"Pipe", H, "L", {{"in", IN}, {"out", OUT}},
                   {{"length", "m", 10.0, 0.0, 0.0},
                    {"ID", "m", 0.1, 1e-3, 0.0},
                    {"OD", "m", 0.114, 1e-3, 0.0},
                    {"roughness", "m", 4.5e-5, 0.0, 0.0},
                    {"minorK", "-", 0.0, 0.0, 0.0},
                    {"tuning", "-", 1.0, 0.0, 0.0}}});

  // Control valve sized by metric flow coefficient Kv with an inherent
  // characteristic (0=linear, 1=equal-percentage, 2=quick-opening).
  reg.registerDef({"Valve", H, "FCV", {{"in", IN}, {"out", OUT}},
                   {{"Kv", "m3/h/bar^0.5", 50.0, 1e-3, 0.0},
                    {"characteristic", "0/1/2", 1.0, 0.0, 2.0},
                    {"rangeability", "-", 50.0, 1.1, 0.0},
                    {"position", "-", 1.0, 0.0, 1.0}}});

  // Thin-plate metering orifice (ISO 5167).
  reg.registerDef({"Orifice", H, "FE", {{"in", IN}, {"out", OUT}},
                   {{"bore", "m", 0.05, 1e-3, 0.0},
                    {"pipeID", "m", 0.1, 1e-3, 0.0},
                    {"Cd", "-", 0.62, 0.1, 1.0}}});

  // Centrifugal pump. Uses a fitted (Q,H) "head" curve if present, else builds
  // a quadratic from shutoff/rated datasheet points. speedRatio applies the
  // affinity laws.
  reg.registerDef({"Pump", H, "P", {{"in", IN}, {"out", OUT}},
                   {{"ratedFlow", "m3/s", 0.05, 0.0, 0.0},
                    {"ratedHead", "m", 50.0, 0.0, 0.0},
                    {"shutoffHead", "m", 65.0, 0.0, 0.0},
                    {"efficiency", "-", 0.75, 0.0, 1.0},
                    {"speedRatio", "-", 1.0, 0.0, 0.0}}});

  reg.registerDef({"Junction", H, "J", {{"a", BI}, {"b", BI}, {"c", BI}}, {}});

  // Shell-and-tube heat exchanger (tube-side hydraulics; thermal duty doc'd).
  reg.registerDef({"HeatExchanger", H, "HX", {{"in", IN}, {"out", OUT}},
                   {{"tubeLength", "m", 4.0, 0.0, 0.0},
                    {"tubeID", "m", 0.016, 1e-3, 0.0},
                    {"tubeOD", "m", 0.019, 1e-3, 0.0},
                    {"numTubes", "-", 100.0, 1.0, 0.0},
                    {"numPasses", "-", 2.0, 1.0, 0.0},
                    {"roughness", "m", 1.5e-6, 0.0, 0.0},
                    {"minorK", "-", 2.0, 0.0, 0.0},
                    {"U", "W/m2K", 500.0, 0.0, 0.0}}});

  // ------------- Instrumentation / control (library symbols) --------------
  // These carry tags and configuration and render as ISA symbols, but are not
  // yet coupled into the hydraulic solve (Phase 1). The single bidirectional
  // "sig" port lets them be associated with a process component on the P&ID.

  // Transmitter: measVar 0=Pressure,1=Flow,2=Level,3=Temperature.
  reg.registerDef({"Transmitter", I, "XT", {{"sig", BI}},
                   {{"measVar", "0..3", 0.0, 0.0, 3.0},
                    {"rangeMin", "eu", 0.0, 0.0, 0.0},
                    {"rangeMax", "eu", 100.0, 0.0, 0.0}}});

  // Valve actuator: failPosition 0=closed,1=open; type 0=pneumatic,1=motor.
  reg.registerDef({"Actuator", I, "ACT", {{"sig", BI}},
                   {{"type", "0/1", 0.0, 0.0, 1.0},
                    {"strokeTime", "s", 5.0, 0.0, 0.0},
                    {"failPosition", "-", 0.0, 0.0, 1.0}}});

  // PID controller / instrument bubble.
  reg.registerDef({"Controller", C, "IC", {{"sig", BI}},
                   {{"setpoint", "eu", 50.0, 0.0, 0.0},
                    {"gain", "-", 1.0, 0.0, 0.0},
                    {"Ti", "s", 10.0, 0.0, 0.0},
                    {"Td", "s", 0.0, 0.0, 0.0}}});
}

}  // namespace umpnap

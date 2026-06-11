#include "core/ComponentRegistry.h"

#include <string>

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
  c->fluid = d->defaultFluid;
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
  const Medium SIG = Medium::Signal;       // instrument/control signal ports
  const Medium ELEC = Medium::Electrical;  // electrical terminals
  const Medium GAS = Medium::Gas;          // fixed-gas ports (cover gas)
  const Medium LIQ = Medium::Liquid;       // fixed-liquid ports
  const Medium STM = Medium::Steam;        // steam ports

  // Each ParamSpec is {name, unit, default, min, max}. Values are stored in SI.
  // ----------------------- Hydraulic (solving) ----------------------------

  // Pressure boundary / reservoir. Anchors the network pressure.
  reg.registerDef({"Boundary", H, "BND", {{"p", BI}},
                   {{"pressure", "Pa", 2.0e5, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Vertical cylindrical tank with a liquid port (p) and a cover-gas tapping
  // (gas) so an air/gas network can be drawn to the vapour space.
  reg.registerDef({"Tank", H, "TK",
                   {{"p", BI, Medium::Process}, {"gas", BI, GAS}},
                   {{"diameter", "m", 2.0, 1e-3, 0.0},
                    {"height", "m", 6.0, 0.0, 0.0},
                    {"level", "m", 2.0, 0.0, 0.0},
                    {"p_top", "Pa", 1.013e5, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Pipe: Darcy-Weisbach + minor losses + static head. ID drives flow area;
  // OD is wall info. dZ = outlet elevation - inlet elevation (positive: outlet
  // higher than inlet, i.e. flow climbs from inlet -> outlet).
  reg.registerDef({"Pipe", H, "L", {{"in", IN}, {"out", OUT}},
                   {{"length", "m", 10.0, 0.0, 0.0},
                    {"ID", "m", 0.1, 1e-3, 0.0},
                    {"OD", "m", 0.114, 1e-3, 0.0},
                    {"roughness", "m", 4.5e-5, 0.0, 0.0},
                    {"minorK", "-", 0.0, 0.0, 0.0},
                    {"dZ", "m", 0.0, 0.0, 0.0},
                    {"tuning", "-", 1.0, 0.0, 0.0}}});

  // Control valve sized by metric flow coefficient Kv with an inherent
  // characteristic (0=linear, 1=equal-percentage, 2=quick-opening). The "act"
  // signal port lets a valve actuator (manual/electric/pneumatic) be wired to it.
  reg.registerDef({"Valve", H, "FCV",
                   {{"in", IN}, {"out", OUT}, {"act", BI, SIG}},
                   {{"Kv", "m3/h/bar^0.5", 50.0, 1e-3, 0.0},
                    {"characteristic", "0/1/2", 1.0, 0.0, 2.0},
                    {"rangeability", "-", 50.0, 1.1, 0.0},
                    {"position", "-", 1.0, 0.0, 1.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Thin-plate metering orifice (ISO 5167).
  reg.registerDef({"Orifice", H, "FE", {{"in", IN}, {"out", OUT}},
                   {{"bore", "m", 0.05, 1e-3, 0.0},
                    {"pipeID", "m", 0.1, 1e-3, 0.0},
                    {"Cd", "-", 0.62, 0.1, 1.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Centrifugal pump. Uses a fitted (Q,H) "head" curve if present, else builds
  // a quadratic from shutoff/rated datasheet points. speedRatio applies the
  // affinity laws. The "drive" electrical port accepts a driving Motor.
  reg.registerDef({"Pump", H, "P",
                   {{"in", IN}, {"out", OUT}, {"drive", BI, ELEC}},
                   {{"ratedFlow", "m3/s", 0.05, 0.0, 0.0},
                    {"ratedHead", "m", 50.0, 0.0, 0.0},
                    {"shutoffHead", "m", 65.0, 0.0, 0.0},
                    {"efficiency", "-", 0.75, 0.0, 1.0},
                    {"speedRatio", "-", 1.0, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

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
                    {"U", "W/m2K", 500.0, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Split shell-and-tube exchanger. ShellSide and TubeSide are SEPARATE flow
  // elements (each a hydraulic branch on its own mimic) that exchange heat when
  // they share a non-empty config["thermalTag"]. This lets the two streams live
  // in different subsystems (e.g. shell in process cooling water, tube in the
  // moderator loop) and still couple thermally in the integrated solve. The
  // duty is computed by the effectiveness-NTU method each steady cycle; outlet
  // temperatures are recorded as derived signals. (thermalTag is distinct from
  // linkTag, which denotes a single shared physical instance.)
  // Tube side: same tube-bundle hydraulics as HeatExchanger plus an inlet temp.
  reg.registerDef({"TubeSide", H, "HXT", {{"in", IN}, {"out", OUT}},
                   {{"tubeLength", "m", 4.0, 0.0, 0.0},
                    {"tubeID", "m", 0.016, 1e-3, 0.0},
                    {"tubeOD", "m", 0.019, 1e-3, 0.0},
                    {"numTubes", "-", 100.0, 1.0, 0.0},
                    {"numPasses", "-", 2.0, 1.0, 0.0},
                    {"roughness", "m", 1.5e-6, 0.0, 0.0},
                    {"minorK", "-", 2.0, 0.0, 0.0},
                    {"U", "W/m2K", 500.0, 0.0, 0.0},
                    {"Tin", "C", 90.0, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});
  // Shell side: shell crossflow resistance (Kern-style) plus an inlet temp.
  reg.registerDef({"ShellSide", H, "HXS", {{"in", IN}, {"out", OUT}},
                   {{"shellID", "m", 0.4, 1e-3, 0.0},
                    {"baffleSpacing", "m", 0.3, 1e-3, 0.0},
                    {"tubeOD", "m", 0.019, 1e-3, 0.0},
                    {"pitchRatio", "-", 1.25, 1.0, 0.0},
                    {"crossLossK", "-", 4.0, 0.0, 0.0},
                    {"minorK", "-", 2.0, 0.0, 0.0},
                    {"Tin", "C", 30.0, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Filter / strainer: clean-element resistance from a rated ΔP at rated flow.
  reg.registerDef({"Filter", H, "FL", {{"in", IN}, {"out", OUT}},
                   {{"ratedFlow", "m3/s", 0.05, 1e-6, 0.0},
                    {"ratedDP", "Pa", 2.0e4, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});
  reg.registerDef({"Strainer", H, "ST", {{"in", IN}, {"out", OUT}},
                   {{"ratedFlow", "m3/s", 0.05, 1e-6, 0.0},
                    {"ratedDP", "Pa", 1.0e4, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Pressure relief / safety valve (self-acting). Stays shut until the
  // differential P_in-P_out reaches `setpoint`, then opens progressively over
  // the `blowdown` band up to its full flow coefficient Kv. Discharge is
  // one-way (no reverse flow), so it also behaves as a check on reverse dP.
  reg.registerDef({"ReliefValve", H, "PSV", {{"in", IN}, {"out", OUT}},
                   {{"setpoint", "Pa", 8.0e5, 0.0, 0.0},
                    {"blowdown", "Pa", 5.0e4, 1.0, 0.0},
                    {"Kv", "m3/h/bar^0.5", 40.0, 1e-3, 0.0},
                    {"position", "-", 0.0, 0.0, 1.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // Header: large manifold node (multi-port, ideal — like a Junction).
  reg.registerDef({"Header", H, "HDR",
                   {{"a", BI}, {"b", BI}, {"c", BI}, {"d", BI}}, {}});

  // Pressurised / gas-blanketed tank: liquid port + cover-gas tapping; the top
  // pressure is the blanket gas pressure.
  reg.registerDef({"PressurizedTank", H, "PTK",
                   {{"p", BI, Medium::Process}, {"gas", BI, GAS}},
                   {{"diameter", "m", 2.0, 1e-3, 0.0},
                    {"height", "m", 6.0, 0.0, 0.0},
                    {"level", "m", 3.0, 0.0, 0.0},
                    {"p_top", "Pa", 5.0e5, 0.0, 0.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}}});

  // ------------------------ Pneumatic / air (solving) ---------------------
  // These reuse the hydraulic laws but default to Air as the working fluid:
  // a Duct is a pipe, a Damper a valve, a Fan/Blower/Compressor a pump.
  reg.registerDef({"Duct", H, "DCT", {{"in", IN}, {"out", OUT}},
                   {{"length", "m", 10.0, 0.0, 0.0},
                    {"ID", "m", 0.3, 1e-3, 0.0},
                    {"OD", "m", 0.31, 1e-3, 0.0},
                    {"roughness", "m", 9.0e-5, 0.0, 0.0},
                    {"minorK", "-", 0.0, 0.0, 0.0},
                    {"dZ", "m", 0.0, 0.0, 0.0},
                    {"tuning", "-", 1.0, 0.0, 0.0}},
                   "Air"});
  reg.registerDef({"Damper", H, "DMP",
                   {{"in", IN}, {"out", OUT}, {"act", BI, SIG}},
                   {{"Kv", "m3/h/bar^0.5", 500.0, 1e-3, 0.0},
                    {"characteristic", "0/1/2", 0.0, 0.0, 2.0},
                    {"rangeability", "-", 30.0, 1.1, 0.0},
                    {"position", "-", 1.0, 0.0, 1.0}},
                   "Air"});
  reg.registerDef({"Fan", H, "FAN",
                   {{"in", IN}, {"out", OUT}, {"drive", BI, ELEC}},
                   {{"ratedFlow", "m3/s", 2.0, 0.0, 0.0},
                    {"ratedHead", "m", 30.0, 0.0, 0.0},
                    {"shutoffHead", "m", 40.0, 0.0, 0.0},
                    {"efficiency", "-", 0.7, 0.0, 1.0},
                    {"speedRatio", "-", 1.0, 0.0, 0.0}},
                   "Air"});
  reg.registerDef({"Blower", H, "BLW",
                   {{"in", IN}, {"out", OUT}, {"drive", BI, ELEC}},
                   {{"ratedFlow", "m3/s", 1.0, 0.0, 0.0},
                    {"ratedHead", "m", 80.0, 0.0, 0.0},
                    {"shutoffHead", "m", 100.0, 0.0, 0.0},
                    {"efficiency", "-", 0.7, 0.0, 1.0},
                    {"speedRatio", "-", 1.0, 0.0, 0.0}},
                   "Air"});
  reg.registerDef({"Compressor", H, "CMP",
                   {{"in", IN}, {"out", OUT}, {"drive", BI, ELEC}},
                   {{"ratedFlow", "m3/s", 0.5, 0.0, 0.0},
                    {"ratedHead", "m", 300.0, 0.0, 0.0},
                    {"shutoffHead", "m", 400.0, 0.0, 0.0},
                    {"efficiency", "-", 0.75, 0.0, 1.0},
                    {"speedRatio", "-", 1.0, 0.0, 0.0}},
                   "Air"});
  // Air receiver: a large plenum tying many compressors and consumers together.
  // It exposes 20 interchangeable inflow/outflow tappings (p, n1..n19); the
  // solver collapses them all onto one vessel pressure node. Set a shared
  // `linkTag` to reference the SAME receiver across several mimic files.
  {
    std::vector<Port> arcPorts;
    arcPorts.push_back({"p", BI});
    for (int i = 1; i <= 19; ++i)
      arcPorts.push_back({"n" + std::to_string(i), BI});
    reg.registerDef({"AirReceiver", H, "ARC", arcPorts,
                     {{"diameter", "m", 1.0, 1e-3, 0.0},
                      {"height", "m", 2.5, 0.0, 0.0},
                      {"level", "m", 0.0, 0.0, 0.0},
                      {"p_top", "Pa", 7.0e5, 0.0, 0.0},
                      {"elevation", "m", 0.0, 0.0, 0.0}},
                     "Air"});
  }

  // Air/gas safety relief valve: protects receivers and pneumatic lines. Same
  // self-acting law as the hydraulic relief valve but defaulting to a gas fluid.
  reg.registerDef({"GasReliefValve", H, "PSV", {{"in", IN}, {"out", OUT}},
                   {{"setpoint", "Pa", 8.5e5, 0.0, 0.0},
                    {"blowdown", "Pa", 5.0e4, 1.0, 0.0},
                    {"Kv", "m3/h/bar^0.5", 300.0, 1e-3, 0.0},
                    {"position", "-", 0.0, 0.0, 1.0},
                    {"elevation", "m", 0.0, 0.0, 0.0}},
                   "Air"});

  // ----------------------- Electrical (library symbols) -------------------
  // Rendered and configurable; a power-flow solver is a later phase.
  const Domain E = Domain::Electrical;
  reg.registerDef({"Grid", E, "GRID", {{"t", BI, ELEC}},
                   {{"voltage", "V", 11000.0, 0.0, 0.0},
                    {"frequency", "Hz", 50.0, 0.0, 0.0}}});
  reg.registerDef({"Generator", E, "GEN", {{"t", BI, ELEC}},
                   {{"rating", "kVA", 1000.0, 0.0, 0.0},
                    {"voltage", "V", 415.0, 0.0, 0.0},
                    {"pf", "-", 0.8, 0.0, 1.0}}});
  reg.registerDef({"Transformer", E, "TX", {{"hv", BI, ELEC}, {"lv", BI, ELEC}},
                   {{"rating", "kVA", 1000.0, 0.0, 0.0},
                    {"ratio", "-", 26.5, 0.0, 0.0},
                    {"impedance", "%", 6.0, 0.0, 0.0}}});
  reg.registerDef({"Busbar", E, "BUS",
                   {{"a", BI, ELEC}, {"b", BI, ELEC}, {"c", BI, ELEC}},
                   {{"voltage", "V", 415.0, 0.0, 0.0}}});
  reg.registerDef({"Breaker", E, "CB", {{"in", BI, ELEC}, {"out", BI, ELEC}},
                   {{"rating", "A", 630.0, 0.0, 0.0},
                    {"state", "0/1", 1.0, 0.0, 1.0}}});
  reg.registerDef({"Cable", E, "W", {{"in", BI, ELEC}, {"out", BI, ELEC}},
                   {{"length", "m", 50.0, 0.0, 0.0},
                    {"area", "mm2", 95.0, 0.0, 0.0}}});
  reg.registerDef({"Motor", E, "M", {{"t", BI, ELEC}},
                   {{"rating", "kW", 75.0, 0.0, 0.0},
                    {"voltage", "V", 415.0, 0.0, 0.0},
                    {"efficiency", "-", 0.92, 0.0, 1.0}}});
  reg.registerDef({"ElectricalLoad", E, "LD", {{"t", BI, ELEC}},
                   {{"power", "kW", 50.0, 0.0, 0.0},
                    {"pf", "-", 0.85, 0.0, 1.0}}});

  // ------------- Instrumentation / control (library symbols) --------------
  // These carry tags and configuration and render as ISA symbols, but are not
  // yet coupled into the hydraulic solve (Phase 1). The single bidirectional
  // "sig" port lets them be associated with a process component on the P&ID.

  // Transmitter: measVar 0=Pressure,1=Flow,2=Level,3=Temperature.
  reg.registerDef({"Transmitter", I, "XT", {{"sig", BI, SIG}},
                   {{"measVar", "0..3", 0.0, 0.0, 3.0},
                    {"rangeMin", "eu", 0.0, 0.0, 0.0},
                    {"rangeMax", "eu", 100.0, 0.0, 0.0}}});

  // Valve actuator: failPosition 0=closed,1=open; type 0=pneumatic,1=motor.
  // Wire the "sig" port to a valve/damper "act" port to record the drive link.
  reg.registerDef({"Actuator", I, "ACT", {{"sig", BI, SIG}},
                   {{"type", "0/1", 0.0, 0.0, 1.0},
                    {"strokeTime", "s", 5.0, 0.0, 0.0},
                    {"failPosition", "-", 0.0, 0.0, 1.0}}});

  // Typed valve actuators. All wire their "sig" port to a valve/damper "act"
  // port. modulating=1 positions continuously (throttling); modulating=0 is
  // open/shut (on-off). failPosition 0=closed, 1=open on loss of motive power.
  // Electric (motor-operated, MOV).
  reg.registerDef({"ElectricActuator", I, "MOV", {{"sig", BI, SIG}},
                   {{"modulating", "0/1", 1.0, 0.0, 1.0},
                    {"strokeTime", "s", 30.0, 0.0, 0.0},
                    {"failPosition", "-", 0.0, 0.0, 1.0}}});
  // Manual handwheel: operator-set, no automatic action.
  reg.registerDef({"ManualActuator", I, "HW", {{"sig", BI, SIG}},
                   {{"turnsToOpen", "-", 12.0, 0.0, 0.0}}});
  // Pneumatic diaphragm actuator with positioner: continuous modulation.
  reg.registerDef({"PneumaticActuatorModulating", I, "PA", {{"sig", BI, SIG}},
                   {{"modulating", "0/1", 1.0, 1.0, 1.0},
                    {"strokeTime", "s", 4.0, 0.0, 0.0},
                    {"supplyP", "Pa", 4.0e5, 0.0, 0.0},
                    {"failPosition", "-", 0.0, 0.0, 1.0}}});
  // Pneumatic on/off actuator (solenoid piloted): open or shut only.
  reg.registerDef({"PneumaticActuatorOnOff", I, "PAO", {{"sig", BI, SIG}},
                   {{"modulating", "0/1", 0.0, 0.0, 0.0},
                    {"strokeTime", "s", 1.5, 0.0, 0.0},
                    {"supplyP", "Pa", 4.0e5, 0.0, 0.0},
                    {"failPosition", "-", 0.0, 0.0, 1.0}}});

  // Process switch (pressure/level/flow/temp): trips at a setpoint.
  reg.registerDef({"Switch", I, "XS", {{"sig", BI, SIG}},
                   {{"measVar", "0..3", 0.0, 0.0, 3.0},
                    {"setpoint", "eu", 50.0, 0.0, 0.0},
                    {"deadband", "eu", 1.0, 0.0, 0.0}}});

  // RTD / thermocouple temperature element.
  reg.registerDef({"RTD", I, "TE", {{"sig", BI, SIG}},
                   {{"rangeMin", "C", 0.0, 0.0, 0.0},
                    {"rangeMax", "C", 200.0, 0.0, 0.0}}});

  // Local indicating gauge.
  reg.registerDef({"Gauge", I, "GI", {{"sig", BI, SIG}},
                   {{"rangeMin", "eu", 0.0, 0.0, 0.0},
                    {"rangeMax", "eu", 16.0, 0.0, 0.0}}});

  // PID controller / instrument bubble.
  reg.registerDef({"Controller", C, "IC", {{"sig", BI, SIG}},
                   {{"setpoint", "eu", 50.0, 0.0, 0.0},
                    {"gain", "-", 1.0, 0.0, 0.0},
                    {"Ti", "s", 10.0, 0.0, 0.0},
                    {"Td", "s", 0.0, 0.0, 0.0}}});

  // Discrete logic / timer blocks.
  reg.registerDef({"Timer", C, "TMR", {{"sig", BI, SIG}},
                   {{"preset", "s", 5.0, 0.0, 0.0}}});
  reg.registerDef({"Logic", C, "LGC", {{"in", BI, SIG}, {"out", BI, SIG}},
                   {{"function", "0=AND/1=OR/2=NOT", 0.0, 0.0, 2.0}}});

  // ------------------------- Steam library (models) ----------------------
  // Multi-medium two-phase equipment. Ports carry the correct media so only
  // valid connections are allowed; full two-phase solving is a later phase.
  reg.registerDef({"SteamGenerator", H, "SG",
                   {{"feedwater", IN, LIQ}, {"steam", OUT, STM},
                    {"blowdown", OUT, LIQ}},
                   {{"pressure", "Pa", 4.7e6, 0.0, 0.0},
                    {"thermalPower", "MW", 600.0, 0.0, 0.0},
                    {"tubeArea", "m2", 3000.0, 0.0, 0.0},
                    {"level", "m", 12.0, 0.0, 0.0}}});
  reg.registerDef({"Turbine", H, "TUR",
                   {{"steam", IN, STM}, {"exhaust", OUT, STM}},
                   {{"ratedPower", "MW", 700.0, 0.0, 0.0},
                    {"inletP", "Pa", 4.5e6, 0.0, 0.0},
                    {"exhaustP", "Pa", 5.0e3, 0.0, 0.0},
                    {"efficiency", "-", 0.85, 0.0, 1.0}}});
  reg.registerDef({"Condenser", H, "CND",
                   {{"steam", IN, STM}, {"condensate", OUT, LIQ},
                    {"cwIn", IN, Medium::Process}, {"cwOut", OUT, Medium::Process}},
                   {{"pressure", "Pa", 5.0e3, 0.0, 0.0},
                    {"duty", "MW", 700.0, 0.0, 0.0},
                    {"area", "m2", 8000.0, 0.0, 0.0}}});
  reg.registerDef({"Deaerator", H, "DA",
                   {{"feedwater", IN, LIQ}, {"steam", IN, STM}, {"out", OUT, LIQ},
                    {"gas", BI, GAS}},
                   {{"pressure", "Pa", 1.2e5, 0.0, 0.0},
                    {"diameter", "m", 3.0, 1e-3, 0.0},
                    {"level", "m", 2.0, 0.0, 0.0},
                    {"p_top", "Pa", 1.2e5, 0.0, 0.0}}});
  // ASDV (atmospheric steam dump) / CSDV (condenser steam dump) relief valves.
  reg.registerDef({"ASDV", H, "ASDV", {{"in", IN, STM}, {"out", OUT, STM}},
                   {{"Kv", "m3/h/bar^0.5", 1500.0, 1e-3, 0.0},
                    {"setpoint", "Pa", 5.0e6, 0.0, 0.0},
                    {"position", "-", 0.0, 0.0, 1.0}}});
  reg.registerDef({"CSDV", H, "CSDV", {{"in", IN, STM}, {"out", OUT, STM}},
                   {{"Kv", "m3/h/bar^0.5", 2000.0, 1e-3, 0.0},
                    {"setpoint", "Pa", 4.8e6, 0.0, 0.0},
                    {"position", "-", 0.0, 0.0, 1.0}}});
}

}  // namespace umpnap

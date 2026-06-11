#include "core/Network.h"

#include "core/ComponentRegistry.h"
#include "core/FluidLibrary.h"

namespace umpnap {

const char* domainName(Domain d) {
  switch (d) {
    case Domain::Hydraulic: return "Hydraulic";
    case Domain::Gas: return "Gas";
    case Domain::Thermal: return "Thermal";
    case Domain::Electrical: return "Electrical";
    case Domain::Control: return "Control";
    case Domain::Instrument: return "Instrument";
  }
  return "Hydraulic";
}

Domain domainFromName(const std::string& s) {
  if (s == "Gas") return Domain::Gas;
  if (s == "Thermal") return Domain::Thermal;
  if (s == "Electrical") return Domain::Electrical;
  if (s == "Control") return Domain::Control;
  if (s == "Instrument") return Domain::Instrument;
  return Domain::Hydraulic;
}

Medium fluidMedium(const std::string& fluid) {
  return FluidLibrary::isGas(fluid) ? Medium::Gas : Medium::Liquid;
}

Medium effectiveMedium(const Component& c, const std::string& portName) {
  const Port* p = c.port(portName);
  if (!p) return Medium::Process;
  if (p->medium != Medium::Process) return p->medium;  // fixed-medium port
  // Process port: medium follows the working fluid. The tank cover-gas tapping
  // carries the configured blanket gas.
  bool tank = c.type == "Tank" || c.type == "PressurizedTank" ||
              c.type == "AirReceiver";
  if (tank && portName == "gas") {
    std::string g = c.cfg("gasFluid");
    return fluidMedium(g.empty() ? "Nitrogen" : g);
  }
  return fluidMedium(c.fluid);
}

int Network::addComponent(std::unique_ptr<Component> c) {
  int id = nextId_++;
  c->id = id;
  // Assign a default P&ID tag (e.g. "P-3") if the caller did not set one.
  if (c->name.empty()) {
    const ComponentDef* def = ComponentRegistry::instance().find(c->type);
    std::string prefix =
        (def && !def->tagPrefix.empty()) ? def->tagPrefix : c->type;
    c->name = prefix + "-" + std::to_string(id);
  }
  comps_.push_back(std::move(c));
  return id;
}

void Network::removeComponent(int id) {
  for (auto it = conns_.begin(); it != conns_.end();) {
    if (it->compA == id || it->compB == id)
      it = conns_.erase(it);
    else
      ++it;
  }
  for (auto it = comps_.begin(); it != comps_.end(); ++it) {
    if ((*it)->id == id) {
      comps_.erase(it);
      return;
    }
  }
}

Component* Network::component(int id) {
  for (auto& c : comps_)
    if (c->id == id) return c.get();
  return nullptr;
}

const Component* Network::component(int id) const {
  for (auto& c : comps_)
    if (c->id == id) return c.get();
  return nullptr;
}

Component* Network::componentByName(const std::string& name) {
  for (auto& c : comps_)
    if (c->name == name) return c.get();
  return nullptr;
}

void Network::connect(int a, const std::string& pa, int b, const std::string& pb) {
  conns_.push_back({a, pa, b, pb});
}

void Network::disconnect(int index) {
  if (index >= 0 && index < (int)conns_.size())
    conns_.erase(conns_.begin() + index);
}

void Network::clear() {
  comps_.clear();
  conns_.clear();
  nextId_ = 1;
}

void Network::copyFrom(const Network& other) {
  comps_.clear();
  for (const auto& c : other.comps_)
    comps_.push_back(std::make_unique<Component>(*c));
  conns_ = other.conns_;
  nextId_ = other.nextId_;
}

std::unique_ptr<Network> Network::clone() const {
  auto n = std::make_unique<Network>();
  n->copyFrom(*this);
  return n;
}

std::string Network::validate() const {
  if (comps_.empty()) return "Network is empty.";

  auto isAnchor = [](const std::string& t) {
    return t == "Boundary" || t == "Tank" || t == "PressurizedTank" ||
           t == "AirReceiver";
  };
  bool hasAnchor = false;
  for (const auto& c : comps_)
    if (isAnchor(c->type)) hasAnchor = true;
  if (!hasAnchor)
    return "Network needs at least one Boundary or Tank to anchor pressure.";

  // Every Inlet/Outlet port must be connected. Anchors and bidirectional
  // (junction/header) ports may be left open.
  for (const auto& c : comps_) {
    bool anchor = isAnchor(c->type);
    for (const auto& port : c->ports) {
      if (anchor) continue;
      if (port.role == PortRole::Bidirectional) continue;  // junction-like
      bool connected = false;
      for (const auto& conn : conns_) {
        if ((conn.compA == c->id && conn.portA == port.name) ||
            (conn.compB == c->id && conn.portB == port.name)) {
          connected = true;
          break;
        }
      }
      if (!connected)
        return c->type + " #" + std::to_string(c->id) + " port '" + port.name +
               "' is not connected.";
    }
  }
  return "";
}

}  // namespace umpnap

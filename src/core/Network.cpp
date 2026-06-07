#include "core/Network.h"

#include "core/ComponentRegistry.h"

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

std::string Network::validate() const {
  if (comps_.empty()) return "Network is empty.";

  bool hasAnchor = false;
  for (const auto& c : comps_)
    if (c->type == "Boundary" || c->type == "Tank") hasAnchor = true;
  if (!hasAnchor)
    return "Network needs at least one Boundary or Tank to anchor pressure.";

  // Every Inlet/Outlet port must be connected. Anchors (Boundary/Tank) and
  // Junction extra ports may be left open.
  for (const auto& c : comps_) {
    bool anchor = (c->type == "Boundary" || c->type == "Tank");
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

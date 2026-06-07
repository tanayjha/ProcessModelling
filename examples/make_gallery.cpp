// Places one of each new library type in a grid so every P&ID symbol can be
// visually verified. Not a runnable network -- symbols only.
#include <cstdio>

#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"

using namespace umpnap;

int main(int argc, char** argv) {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  Network net;
  const char* types[] = {
      "PressurizedTank", "AirReceiver", "Header",   "Filter",    "Strainer",
      "Transformer",     "Busbar",      "Breaker",  "Generator", "Grid",
      "Motor",           "ElectricalLoad", "Cable", "Transmitter", "Switch",
      "RTD",             "Gauge",       "Controller", "Timer",   "Logic",
      "Actuator"};
  int col = 0, row = 0;
  for (const char* t : types) {
    auto c = reg.create(t);
    if (!c) continue;
    c->x = 40 + col * 150;
    c->y = 40 + row * 140;
    net.addComponent(std::move(c));
    if (++col == 5) { col = 0; ++row; }
  }
  const char* path = (argc > 1) ? argv[1] : "examples/gallery.umpnap";
  saveProject(net, path);
  std::printf("Wrote %s with %zu symbols\n", path, net.components().size());
  return 0;
}

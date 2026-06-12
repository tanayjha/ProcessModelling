#include "solver/Malfunctions.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "core/Network.h"

namespace umpnap {

void applyMalfunctions(Network& net) {
  for (auto& c : net.components()) {
    const std::string m = c->cfg("malf");
    if (!m.empty()) {
      if (m == "failOpen") {
        c->params["position"] = 1.0;
      } else if (m == "failClose") {
        c->params["position"] = 0.0;
      } else if (m == "stuck") {
        // Hold at the stored position (default: keep current value).
        std::string v = c->cfg("malfVal");
        if (!v.empty())
          c->params["position"] = std::clamp(std::atof(v.c_str()), 0.0, 1.0);
      } else if (m == "trip") {
        c->params["speedRatio"] = 0.0;
      } else if (m == "open") {
        c->params["state"] = 0.0;  // breaker / switch contact
      } else if (m == "close") {
        c->params["state"] = 1.0;
      }
    }
    // Operator local override wins over both logic and any malfunction.
    const std::string ov = c->cfg("override");
    if (ov == "open")
      c->params["position"] = 1.0;
    else if (ov == "close")
      c->params["position"] = 0.0;
  }
}

}  // namespace umpnap

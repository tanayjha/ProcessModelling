#pragma once
#include <string>

#include "core/Network.h"

namespace umpnap {

// Saves/loads the full network (components + connections) as JSON.
bool saveProject(const Network& net, const std::string& path);
bool loadProject(Network& net, const std::string& path);

}  // namespace umpnap

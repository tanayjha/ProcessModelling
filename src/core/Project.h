#pragma once
#include <string>
#include <vector>

#include "core/Network.h"

namespace umpnap {

// Saves/loads the full network (components + connections) as JSON.
bool saveProject(const Network& net, const std::string& path);
bool loadProject(Network& net, const std::string& path);

// ---------------------- Multi-mimic plant projects -------------------------
// A plant project (.umpproj) is a manifest naming several mimic files
// (.umpnap), one per subsystem. They are simulated together as one integrated
// network: components in different mimics that carry the SAME non-empty
// config["linkTag"] denote the same physical equipment (e.g. a shared air
// receiver) and are unified into a single instance on merge. See
// docs/MULTI_MIMIC.md.
struct PlantProject {
  std::string name;
  std::vector<std::string> mimics;  // mimic paths, relative to the .umpproj
};

bool savePlant(const PlantProject& proj, const std::string& path);
bool loadPlant(PlantProject& proj, const std::string& path);

// Merges mimic networks into one integrated plant network. Components sharing a
// non-empty linkTag are unified (first occurrence wins); every connection is
// rewired onto the surviving instances. Subsystems are stacked into separate
// vertical bands so the merged canvas stays readable.
Network mergeMimics(const std::vector<const Network*>& mimics);

// Loads a .umpproj manifest plus each referenced mimic and returns the merged
// integrated network in `out`. Returns false if the manifest cannot be read.
bool loadPlantNetwork(Network& out, const std::string& projPath);

}  // namespace umpnap

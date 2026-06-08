#include "core/FluidLibrary.h"

namespace umpnap {

namespace {
struct Entry { const char* name; FluidProps p; bool gas; };
// Reference values near 20 C, 1 atm. Poisoned moderator solutions are modelled
// as water with a small density bump (dilute boron / gadolinium nitrate).
const Entry kTable[] = {
    {"Light Water",                 {998.0,  1.0e-3,  4182.0, 0.598}, false},
    {"Heavy Water (D2O)",           {1105.0, 1.25e-3, 4220.0, 0.595}, false},
    {"Borated Water (boron)",       {1002.0, 1.02e-3, 4170.0, 0.598}, false},
    {"Gadolinium Nitrate Solution", {1010.0, 1.05e-3, 4150.0, 0.595}, false},
    {"Oil",                         {850.0,  0.08,    1900.0, 0.13},  false},
    {"Air",                         {1.204,  1.82e-5, 1005.0, 0.0257}, true},
    {"Helium",                      {0.1786, 1.96e-5, 5193.0, 0.152}, true},
    {"Nitrogen",                    {1.165,  1.78e-5, 1040.0, 0.026}, true},
};
}  // namespace

const std::vector<std::string>& FluidLibrary::names() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const auto& e : kTable) v.emplace_back(e.name);
    return v;
  }();
  return n;
}

const std::vector<std::string>& FluidLibrary::liquids() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const auto& e : kTable)
      if (!e.gas) v.emplace_back(e.name);
    return v;
  }();
  return n;
}

const std::vector<std::string>& FluidLibrary::gases() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const auto& e : kTable)
      if (e.gas) v.emplace_back(e.name);
    return v;
  }();
  return n;
}

bool FluidLibrary::isGas(const std::string& name) {
  for (const auto& e : kTable)
    if (name == e.name) return e.gas;
  return false;
}

FluidProps FluidLibrary::props(const std::string& name) {
  for (const auto& e : kTable)
    if (name == e.name) return e.p;
  return kTable[0].p;  // default Light Water
}

bool FluidLibrary::has(const std::string& name) {
  for (const auto& e : kTable)
    if (name == e.name) return true;
  return false;
}

}  // namespace umpnap

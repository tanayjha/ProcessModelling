#pragma once
#include <QString>
#include <string>

namespace umpnap {

// Short governing-equation summary for a component type, shown in the property
// panel so the user can see the physics behind each library model. The full
// derivations live in docs/EQUATIONS.md and the model source comments.
QString equationText(const std::string& type);

}  // namespace umpnap

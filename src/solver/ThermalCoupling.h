#pragma once
#include "core/Network.h"

namespace umpnap {

class Results;

// Couples split shell-and-tube exchangers thermally. Any ShellSide and TubeSide
// component that share a non-empty config["thermalTag"] form one exchanger: with
// the per-stream mass flows from the just-completed hydraulic solve, the duty is
// computed by the effectiveness-NTU method and the outlet temperatures recorded.
// Records (per matched pair) for each side's component id:
//   comp.<id>.duty   exchanger heat duty  [W]   (same magnitude both sides)
//   comp.<id>.Tin    stream inlet temperature   [C]
//   comp.<id>.Tout   stream outlet temperature  [C]
// Streams with no flow are skipped (no heat transferred). Called after the
// hydraulic steady solve so the flows are available in `out`.
void applyThermalCoupling(const Network& net, Results& out, double t);

}  // namespace umpnap

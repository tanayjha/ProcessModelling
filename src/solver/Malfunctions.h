#pragma once

namespace umpnap {

class Network;

// Apply instructor-initiated malfunctions and operator local overrides to the
// network's mutable state, in place, just before a solve. Both are stored on
// each Component's string `config` so they persist with the project and can be
// toggled live from the Instructor station:
//
//   config["malf"]   active malfunction code (empty = none):
//                       failOpen / failClose      valve/damper -> position 1 / 0
//                       stuck                      hold position at config["malfVal"]
//                       trip                       pump/fan/blower/compressor -> speed 0
//                       open / close (breaker)     breaker state 0 / 1
//   config["malfVal"] numeric argument for `stuck` (0..1 position), as text.
//   config["override"] operator local override, wins over logic AND malfunction:
//                       open / close              force position 1 / 0
//
// Multiple components may each carry their own malfunction; a single component
// honours malfunction first, then override (so a local override always wins).
void applyMalfunctions(Network& net);

}  // namespace umpnap

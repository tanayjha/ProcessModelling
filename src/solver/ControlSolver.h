#pragma once

namespace umpnap {

class Network;
class Results;

// Evaluates every Controller once per simulation cycle and drives its output
// component's valve/damper position. Each Controller is linked by tag via its
// string config: measComp (tag of the measured component), measVar
// (level|flow|pressure), output (tag of the valve/damper to drive). A discrete
// PI law is applied:  u = Kp*err + (Kp/Ti)*∫err,  err = setpoint - measured,
// clamped to a valve position in [0,1] with simple anti-windup. Reverse-acting
// loops use a negative gain. See docs/EQUATIONS.md (control section).
void applyControls(Network& net, const Results& res, double dt);

}  // namespace umpnap

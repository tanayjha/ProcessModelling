// Steam cycle layout: SG -> Turbine -> Condenser -> Deaerator -> feed pump -> SG.
// Verifies the steam symbols and that multi-medium ports connect correctly
// (steam<->steam, liquid<->liquid). Two-phase solving is a later phase.
#include <cstdio>
#include "core/ComponentRegistry.h"
#include "core/Network.h"
#include "core/Project.h"
using namespace umpnap;
int main(int argc, char** argv) {
  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  Network net;
  auto sg = reg.create("SteamGenerator"); sg->x=80; sg->y=60; int isg=net.addComponent(std::move(sg));
  auto tur= reg.create("Turbine"); tur->x=320; tur->y=60; int itu=net.addComponent(std::move(tur));
  auto cnd= reg.create("Condenser"); cnd->x=560; cnd->y=60; int icn=net.addComponent(std::move(cnd));
  auto da = reg.create("Deaerator"); da->x=560; da->y=300; int ida=net.addComponent(std::move(da));
  auto fwp= reg.create("Pump"); fwp->name="FWP-1"; fwp->x=300; fwp->y=300; int ifw=net.addComponent(std::move(fwp));
  net.connect(isg,"steam",itu,"steam");
  net.connect(itu,"exhaust",icn,"steam");
  net.connect(icn,"condensate",ida,"feedwater");
  net.connect(ida,"out",ifw,"in");
  net.connect(ifw,"out",isg,"feedwater");
  const char* path=(argc>1)?argv[1]:"examples/steam.umpnap";
  saveProject(net, path);
  std::printf("Wrote %s with %zu components, %zu connections\n", path,
              net.components().size(), net.connections().size());
  return 0;
}

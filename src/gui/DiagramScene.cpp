#include "gui/DiagramScene.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPen>
#include <tuple>

#include "components/hydraulic/BranchLaw.h"
#include "core/ComponentRegistry.h"
#include "core/Results.h"
#include "gui/DiagramItems.h"
#include "solver/NodeGraph.h"

namespace umpnap {

namespace {
QString mediumName(Medium m) {
  switch (m) {
    case Medium::Liquid: return "liquid";
    case Medium::Gas: return "gas";
    case Medium::Steam: return "steam";
    case Medium::Electrical: return "electrical";
    case Medium::Signal: return "signal";
    case Medium::Process: return "process";
  }
  return "?";
}

// Live readout string for a component from the latest results.
QString runtimeText(const Component* c, const NodeGraph& g, const Results& res) {
  auto pressureBar = [&](const std::string& port) -> QString {
    int n = g.nodeOf(c->id, port);
    if (n < 0) return QString();
    double pa = res.latest("node." + std::to_string(n) + ".pressure");
    if (pa == 0.0) return QString();
    return QString("P=%1 bar").arg(pa / 1e5, 0, 'f', 2);
  };
  QString flowStr;
  double q = res.latest("comp." + std::to_string(c->id) + ".flow");
  if (isBranch(c->type))
    flowStr = QString("Q=%1 m³/s").arg(q, 0, 'g', 3);

  if (c->type == "Tank" || c->type == "PressurizedTank")
    return QString("L=%1 m\n%2").arg(c->param("level"), 0, 'f', 2).arg(pressureBar("p"));
  if (c->type == "Boundary") return pressureBar("p");
  if (c->type == "Pump") {
    double h = res.latest("comp." + std::to_string(c->id) + ".head");
    return QString("%1\nH=%2 m").arg(flowStr).arg(h, 0, 'f', 1);
  }
  if (c->type == "Valve" || c->type == "Damper")
    return QString("%1\n%2% open").arg(flowStr).arg(c->param("position") * 100.0, 0, 'f', 0);
  if (isBranch(c->type)) return flowStr;
  if (c->domain != Domain::Hydraulic) return QString();
  return pressureBar(c->ports.empty() ? "" : c->ports.front().name);
}

// Signed flow from the A endpoint toward the B endpoint of a connection.
double connectionFlow(const Connection& cn, const Network& net, const Results& res) {
  const Component* a = net.component(cn.compA);
  const Component* b = net.component(cn.compB);
  if (a && isBranch(a->type)) {
    double q = res.latest("comp." + std::to_string(a->id) + ".flow");
    return (cn.portA == "out") ? q : -q;  // out -> flow leaves A toward B
  }
  if (b && isBranch(b->type)) {
    double q = res.latest("comp." + std::to_string(b->id) + ".flow");
    return (cn.portB == "in") ? q : -q;  // in -> flow enters B from A
  }
  return 0.0;
}
}  // namespace

DiagramScene::DiagramScene(Network* net, QObject* parent)
    : QGraphicsScene(parent), net_(net) {
  setSceneRect(0, 0, 1600, 1000);
  connect(this, &QGraphicsScene::selectionChanged, this,
          &DiagramScene::onSelectionChanged);
}

ComponentItem* DiagramScene::itemForId(int id) const {
  for (auto* it : items_)
    if (it->comp()->id == id) return it;
  return nullptr;
}

void DiagramScene::addComponentItem(Component* c) {
  auto* item = new ComponentItem(c);
  addItem(item);
  items_.push_back(item);
}

void DiagramScene::addConnectionItemForIndex(int connIndex) {
  const auto& conns = net_->connections();
  if (connIndex < 0 || connIndex >= (int)conns.size()) return;
  const Connection& cn = conns[connIndex];
  ComponentItem* a = itemForId(cn.compA);
  ComponentItem* b = itemForId(cn.compB);
  if (!a || !b) return;
  int pa = a->portIndexByName(cn.portA);
  int pb = b->portIndexByName(cn.portB);
  if (pa < 0 || pb < 0) return;
  auto* wire = new ConnectionItem(a, pa, b, pb);
  addItem(wire);
  conns_.push_back(wire);
}

void DiagramScene::rebuildFromNetwork() {
  clear();  // deletes all items
  items_.clear();
  conns_.clear();
  for (const auto& c : net_->components()) addComponentItem(c.get());
  for (int i = 0; i < (int)net_->connections().size(); ++i)
    addConnectionItemForIndex(i);
}

void DiagramScene::refreshConnections() {
  for (auto* w : conns_) w->updatePosition();
}

void DiagramScene::updateRuntime(const Results& res) {
  NodeGraph g = buildNodeGraph(*net_);
  for (auto* it : items_) it->setRuntime(runtimeText(it->comp(), g, res));
  const auto& conns = net_->connections();
  for (size_t i = 0; i < conns_.size() && i < conns.size(); ++i)
    conns_[i]->setFlow(connectionFlow(conns[i], *net_, res));
}

void DiagramScene::clearRuntime() {
  for (auto* it : items_) it->setRuntime("");
  for (auto* w : conns_) w->setFlow(0.0);
}

ComponentItem* DiagramScene::portHitTest(const QPointF& scenePos, int& portIdx) const {
  for (auto* it : items_) {
    int p = it->portAt(scenePos);
    if (p >= 0) {
      portIdx = p;
      return it;
    }
  }
  portIdx = -1;
  return nullptr;
}

void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  // Placing a new component from the palette.
  if (!armedType_.isEmpty() && e->button() == Qt::LeftButton) {
    auto c = ComponentRegistry::instance().create(armedType_.toStdString());
    if (c) {
      c->x = e->scenePos().x();
      c->y = e->scenePos().y();
      Component* raw = c.get();
      net_->addComponent(std::move(c));
      addComponentItem(raw);
      emit networkChanged();
    }
    armedType_.clear();
    e->accept();
    return;
  }

  // Starting a wire from a port handle.
  if (e->button() == Qt::LeftButton) {
    int pIdx = -1;
    ComponentItem* hit = portHitTest(e->scenePos(), pIdx);
    if (hit) {
      connecting_ = true;
      srcItem_ = hit;
      srcPort_ = pIdx;
      tempLine_ = new QGraphicsLineItem(
          QLineF(hit->portScenePos(pIdx), e->scenePos()));
      QPen pen(QColor(120, 120, 120));
      pen.setStyle(Qt::DashLine);
      pen.setWidth(2);
      tempLine_->setPen(pen);
      addItem(tempLine_);
      e->accept();
      return;
    }
  }

  QGraphicsScene::mousePressEvent(e);
}

void DiagramScene::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {
  if (connecting_ && tempLine_) {
    tempLine_->setLine(QLineF(srcItem_->portScenePos(srcPort_), e->scenePos()));
    e->accept();
    return;
  }
  QGraphicsScene::mouseMoveEvent(e);
}

void DiagramScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
  if (connecting_) {
    int pIdx = -1;
    ComponentItem* hit = portHitTest(e->scenePos(), pIdx);
    if (tempLine_) {
      removeItem(tempLine_);
      delete tempLine_;
      tempLine_ = nullptr;
    }
    if (hit && hit != srcItem_) {
      PortRole rs = srcItem_->portRole(srcPort_);
      PortRole rd = hit->portRole(pIdx);
      bool bothIn = rs == PortRole::Inlet && rd == PortRole::Inlet;
      bool bothOut = rs == PortRole::Outlet && rd == PortRole::Outlet;
      // Media must match: a liquid port cannot feed a gas/steam/electrical port.
      Medium ms = effectiveMedium(*srcItem_->comp(), srcItem_->portName(srcPort_));
      Medium md = effectiveMedium(*hit->comp(), hit->portName(pIdx));
      if (bothIn || bothOut) {
        emit connectionRejected("Cannot connect two inlets or two outlets.");
      } else if (ms != md) {
        emit connectionRejected("Incompatible media: " + mediumName(ms) + " ↔ " +
                                mediumName(md) + " ports cannot be connected.");
      } else {
        net_->connect(srcItem_->comp()->id, srcItem_->portName(srcPort_),
                      hit->comp()->id, hit->portName(pIdx));
        addConnectionItemForIndex((int)net_->connections().size() - 1);
        emit networkChanged();
      }
    }
    connecting_ = false;
    srcItem_ = nullptr;
    srcPort_ = -1;
    e->accept();
    return;
  }
  QGraphicsScene::mouseReleaseEvent(e);
}

void DiagramScene::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
    std::vector<int> compsToDelete;
    // Connections to remove, as endpoint tuples (component id + port name).
    std::vector<std::tuple<int, std::string, int, std::string>> wires;
    for (auto* it : selectedItems()) {
      if (auto* ci = dynamic_cast<ComponentItem*>(it))
        compsToDelete.push_back(ci->comp()->id);
      else if (auto* w = dynamic_cast<ConnectionItem*>(it))
        wires.emplace_back(w->endA()->comp()->id, w->endA()->portName(w->portA()),
                           w->endB()->comp()->id, w->endB()->portName(w->portB()));
    }
    if (!compsToDelete.empty() || !wires.empty()) {
      // Remove selected wires by matching endpoints.
      for (const auto& wr : wires) {
        const auto& conns = net_->connections();
        for (int i = 0; i < (int)conns.size(); ++i) {
          const Connection& c = conns[i];
          bool fwd = c.compA == std::get<0>(wr) && c.portA == std::get<1>(wr) &&
                     c.compB == std::get<2>(wr) && c.portB == std::get<3>(wr);
          bool rev = c.compA == std::get<2>(wr) && c.portA == std::get<3>(wr) &&
                     c.compB == std::get<0>(wr) && c.portB == std::get<1>(wr);
          if (fwd || rev) { net_->disconnect(i); break; }
        }
      }
      for (int id : compsToDelete) net_->removeComponent(id);
      rebuildFromNetwork();
      emit networkChanged();
      if (!compsToDelete.empty()) emit componentSelected(nullptr);
      e->accept();
      return;
    }
  }
  QGraphicsScene::keyPressEvent(e);
}

void DiagramScene::selectComponent(int id) {
  clearSelection();
  if (auto* it = itemForId(id)) it->setSelected(true);
}

void DiagramScene::onSelectionChanged() {
  for (auto* it : selectedItems()) {
    if (auto* ci = dynamic_cast<ComponentItem*>(it)) {
      emit componentSelected(ci->comp());
      return;
    }
  }
  emit componentSelected(nullptr);
}

}  // namespace umpnap

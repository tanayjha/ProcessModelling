#include "gui/DiagramScene.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPen>

#include "core/ComponentRegistry.h"
#include "gui/DiagramItems.h"

namespace umpnap {

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
      if (!bothIn && !bothOut) {
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
    std::vector<int> toDelete;
    for (auto* it : selectedItems()) {
      if (auto* ci = dynamic_cast<ComponentItem*>(it))
        toDelete.push_back(ci->comp()->id);
    }
    if (!toDelete.empty()) {
      for (int id : toDelete) net_->removeComponent(id);
      rebuildFromNetwork();
      emit networkChanged();
      emit componentSelected(nullptr);
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

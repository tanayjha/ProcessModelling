#include "gui/DiagramItems.h"

#include <QFont>
#include <QPainter>
#include <QPen>
#include <cmath>

#include "gui/DiagramScene.h"

namespace umpnap {

namespace {
QColor portColor(PortRole r) {
  switch (r) {
    case PortRole::Inlet: return QColor(46, 160, 67);    // green
    case PortRole::Outlet: return QColor(207, 60, 60);   // red
    case PortRole::Bidirectional: return QColor(56, 120, 220);  // blue
  }
  return Qt::gray;
}
QColor domainColor(Domain d) {
  switch (d) {
    case Domain::Hydraulic: return QColor(220, 235, 250);
    case Domain::Gas: return QColor(245, 240, 220);
    case Domain::Thermal: return QColor(250, 225, 220);
    case Domain::Electrical: return QColor(235, 245, 225);
    case Domain::Control: return QColor(235, 230, 245);
  }
  return QColor(235, 235, 235);
}
}  // namespace

ComponentItem::ComponentItem(Component* c) : comp_(c) {
  setFlag(ItemIsMovable, true);
  setFlag(ItemIsSelectable, true);
  setFlag(ItemSendsGeometryChanges, true);
  setPos(c->x, c->y);
  setZValue(1);
  layoutPorts();
}

void ComponentItem::layoutPorts() {
  ports_.clear();
  std::vector<int> inlets, outlets, bidir;
  for (int i = 0; i < (int)comp_->ports.size(); ++i) {
    switch (comp_->ports[i].role) {
      case PortRole::Inlet: inlets.push_back(i); break;
      case PortRole::Outlet: outlets.push_back(i); break;
      case PortRole::Bidirectional: bidir.push_back(i); break;
    }
  }
  auto place = [&](const std::vector<int>& idxs, double xLocal, bool bottom) {
    int n = (int)idxs.size();
    for (int k = 0; k < n; ++k) {
      const Port& port = comp_->ports[idxs[k]];
      PortVis pv;
      pv.name = port.name;
      pv.role = port.role;
      if (bottom) {
        double x = w_ * (k + 1.0) / (n + 1.0);
        pv.local = QPointF(x, h_);
      } else {
        double y = h_ * (k + 1.0) / (n + 1.0);
        pv.local = QPointF(xLocal, y);
      }
      ports_.push_back(pv);
    }
  };
  place(inlets, 0.0, false);
  place(outlets, w_, false);
  place(bidir, 0.0, true);
}

QRectF ComponentItem::boundingRect() const {
  return QRectF(-10, -10, w_ + 20, h_ + 24);
}

void ComponentItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  QRectF body(0, 0, w_, h_);
  p->setRenderHint(QPainter::Antialiasing, true);
  QPen border(isSelected() ? QColor(20, 20, 20) : QColor(90, 90, 90));
  border.setWidth(isSelected() ? 3 : 1);
  p->setPen(border);
  p->setBrush(domainColor(comp_->domain));
  p->drawRoundedRect(body, 8, 8);

  p->setPen(QColor(20, 20, 20));
  QFont f = p->font();
  f.setBold(true);
  p->setFont(f);
  p->drawText(body.adjusted(6, 4, -6, -h_ / 2),
              Qt::AlignLeft | Qt::AlignTop, QString::fromStdString(comp_->type));
  f.setBold(false);
  p->setFont(f);
  QRectF infoRect = body.adjusted(6, h_ / 2 - 6, -14, -4);
  QString info = "#" + QString::number(comp_->id) + "  " +
                 QString::fromStdString(comp_->fluid);
  QString elided = p->fontMetrics().elidedText(info, Qt::ElideRight,
                                               (int)infoRect.width());
  p->drawText(infoRect, Qt::AlignLeft | Qt::AlignTop, elided);

  for (const auto& pv : ports_) {
    p->setBrush(portColor(pv.role));
    p->setPen(QPen(Qt::black, 1));
    p->drawEllipse(pv.local, 5, 5);
  }
}

int ComponentItem::portAt(const QPointF& scenePos) const {
  for (int i = 0; i < (int)ports_.size(); ++i) {
    QPointF sp = mapToScene(ports_[i].local);
    double dx = sp.x() - scenePos.x();
    double dy = sp.y() - scenePos.y();
    if (std::sqrt(dx * dx + dy * dy) < 9.0) return i;
  }
  return -1;
}

QPointF ComponentItem::portScenePos(int idx) const {
  if (idx < 0 || idx >= (int)ports_.size()) return mapToScene(QPointF(w_ / 2, h_ / 2));
  return mapToScene(ports_[idx].local);
}

int ComponentItem::portIndexByName(const std::string& name) const {
  for (int i = 0; i < (int)ports_.size(); ++i)
    if (ports_[i].name == name) return i;
  return -1;
}

const std::string& ComponentItem::portName(int idx) const {
  static const std::string empty;
  if (idx < 0 || idx >= (int)ports_.size()) return empty;
  return ports_[idx].name;
}

PortRole ComponentItem::portRole(int idx) const {
  if (idx < 0 || idx >= (int)ports_.size()) return PortRole::Bidirectional;
  return ports_[idx].role;
}

QVariant ComponentItem::itemChange(GraphicsItemChange change, const QVariant& value) {
  if (change == ItemPositionHasChanged) {
    comp_->x = pos().x();
    comp_->y = pos().y();
    if (auto* s = dynamic_cast<DiagramScene*>(scene())) s->refreshConnections();
  }
  return QGraphicsItem::itemChange(change, value);
}

// ------------------------- ConnectionItem -------------------------

ConnectionItem::ConnectionItem(ComponentItem* a, int pa, ComponentItem* b, int pb)
    : a_(a), pa_(pa), b_(b), pb_(pb) {
  QPen pen(QColor(70, 70, 70));
  pen.setWidth(2);
  setPen(pen);
  setZValue(-1);
  updatePosition();
}

void ConnectionItem::updatePosition() {
  setLine(QLineF(a_->portScenePos(pa_), b_->portScenePos(pb_)));
}

}  // namespace umpnap

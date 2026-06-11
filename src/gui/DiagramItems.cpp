#include "gui/DiagramItems.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <cmath>

#include "gui/DiagramScene.h"

namespace umpnap {

namespace {
QColor portColor(PortRole r) {
  switch (r) {
    case PortRole::Inlet: return QColor(46, 160, 67);          // green
    case PortRole::Outlet: return QColor(207, 60, 60);         // red
    case PortRole::Bidirectional: return QColor(56, 120, 220); // blue
  }
  return Qt::gray;
}

// Draw the standard P&ID symbol for a component type inside rect g.
void drawSymbol(QPainter* p, const std::string& type, const Component& c,
                const QRectF& g) {
  p->setRenderHint(QPainter::Antialiasing, true);
  QPen line(QColor(40, 40, 40));
  line.setWidth(2);
  p->setPen(line);
  p->setBrush(QColor(255, 255, 255));

  const double cx = g.center().x(), cy = g.center().y();
  const double r = std::min(g.width(), g.height()) / 2.0 - 2.0;

  if (type == "Boundary") {
    // Source/sink: circle.
    p->drawEllipse(QPointF(cx, cy), r, r);

  } else if (type == "AirReceiver") {
    // Horizontal pressure drum spanning the wide footprint (many nozzles).
    QRectF body(g.left() + 4, cy - r * 0.7, g.width() - 8, r * 1.4);
    p->drawRoundedRect(body, r * 0.6, r * 0.6);
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->drawText(body, Qt::AlignCenter, "AIR");

  } else if (type == "Tank" || type == "PressurizedTank") {
    // Vertical cylinder with a liquid level line.
    QRectF body(cx - r, g.top() + 2, 2 * r, g.height() - 4);
    double ry = std::min(8.0, body.height() / 4);
    p->drawRoundedRect(body, ry, ry);
    double lvl = c.param("level");
    double h = std::max(0.001, c.param("height"));
    double frac = std::min(1.0, lvl / h);
    double yLine = body.bottom() - frac * (body.height() - 4) - 2;
    QPen blue(QColor(56, 120, 220));
    blue.setWidth(2);
    p->setPen(blue);
    p->drawLine(QPointF(body.left() + 2, yLine), QPointF(body.right() - 2, yLine));
    p->fillRect(QRectF(body.left() + 2, yLine, body.width() - 4,
                       body.bottom() - yLine - 1),
                QColor(56, 120, 220, 40));

  } else if (type == "Pump" || type == "Fan" || type == "Blower" ||
             type == "Compressor") {
    // Rotodynamic machine: circle with an impeller triangle toward the outlet.
    p->drawEllipse(QPointF(cx, cy), r, r);
    QPolygonF tri;
    tri << QPointF(cx - r * 0.4, cy - r * 0.55)
        << QPointF(cx - r * 0.4, cy + r * 0.55) << QPointF(cx + r * 0.7, cy);
    p->setBrush(QColor(220, 220, 220));
    p->drawPolygon(tri);

  } else if (type == "Pipe" || type == "Duct") {
    // Pipe/duct spool: two parallel run lines with flange ticks at each end.
    double off = std::min(8.0, r * 0.5);
    p->drawLine(QPointF(g.left(), cy - off), QPointF(g.right(), cy - off));
    p->drawLine(QPointF(g.left(), cy + off), QPointF(g.right(), cy + off));
    p->drawLine(QPointF(g.left() + 6, cy - off - 4), QPointF(g.left() + 6, cy + off + 4));
    p->drawLine(QPointF(g.right() - 6, cy - off - 4), QPointF(g.right() - 6, cy + off + 4));

  } else if (type == "Valve" || type == "Damper" || type == "ASDV" ||
             type == "CSDV") {
    // Bowtie (two triangles meeting at the stem centre).
    QPolygonF bow;
    bow << QPointF(g.left() + 2, cy - r) << QPointF(cx, cy)
        << QPointF(g.left() + 2, cy + r);
    p->drawPolygon(bow);
    QPolygonF bow2;
    bow2 << QPointF(g.right() - 2, cy - r) << QPointF(cx, cy)
         << QPointF(g.right() - 2, cy + r);
    p->drawPolygon(bow2);

  } else if (type == "ReliefValve" || type == "GasReliefValve") {
    // Angle relief valve: a bowtie body with a spring coil on the bonnet.
    QPolygonF bow;
    bow << QPointF(g.left() + 2, cy - r * 0.7) << QPointF(cx, cy)
        << QPointF(g.left() + 2, cy + r * 0.7);
    p->drawPolygon(bow);
    QPolygonF bow2;
    bow2 << QPointF(g.right() - 2, cy - r * 0.7) << QPointF(cx, cy)
         << QPointF(g.right() - 2, cy + r * 0.7);
    p->drawPolygon(bow2);
    // Spring zig-zag above the stem (the self-acting set pressure).
    QPainterPath spring;
    double sy0 = g.top() + 2, sy1 = cy - r * 0.2, sx = cx;
    spring.moveTo(sx, sy1);
    int z = 4;
    for (int i = 0; i < z; ++i) {
      double yy = sy1 + (sy0 - sy1) * (i + 1) / z;
      spring.lineTo(sx + ((i % 2) ? 5 : -5), yy);
    }
    p->setBrush(Qt::NoBrush);
    p->drawPath(spring);

  } else if (type == "Orifice") {
    // Pipe run with a thin plate (gap) across it.
    p->drawLine(QPointF(g.left(), cy), QPointF(g.right(), cy));
    QPen plate(QColor(40, 40, 40));
    plate.setWidth(3);
    p->setPen(plate);
    p->drawLine(QPointF(cx, cy - r), QPointF(cx, cy - r * 0.25));
    p->drawLine(QPointF(cx, cy + r * 0.25), QPointF(cx, cy + r));

  } else if (type == "HeatExchanger") {
    // Shell (rounded rect) with an internal tube serpentine.
    QRectF shell(g.left() + 2, cy - r, g.width() - 4, 2 * r);
    p->drawRoundedRect(shell, 6, 6);
    QPainterPath path;
    double x0 = shell.left() + 6, x1 = shell.right() - 6;
    path.moveTo(x0, cy);
    int seg = 4;
    for (int i = 0; i <= seg; ++i) {
      double x = x0 + (x1 - x0) * i / seg;
      double y = cy + ((i % 2 == 0) ? -r * 0.5 : r * 0.5);
      path.lineTo(x, y);
    }
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);

  } else if (type == "SteamGenerator") {
    // Tall vessel with a domed top and U-tube bundle hint.
    QRectF body(cx - r * 0.8, g.top() + 6, r * 1.6, g.height() - 8);
    p->drawRoundedRect(body, r * 0.5, r * 0.5);
    p->drawArc(QRectF(body.left(), body.top() - 6, body.width(), 12), 0, 180 * 16);
    p->drawLine(QPointF(cx - 4, body.center().y()), QPointF(cx - 4, body.bottom() - 4));
    p->drawLine(QPointF(cx + 4, body.center().y()), QPointF(cx + 4, body.bottom() - 4));
    p->drawArc(QRectF(cx - 4, body.center().y() - 4, 8, 8), 180 * 16, 180 * 16);

  } else if (type == "Turbine") {
    // Casing trapezoid (expands toward the exhaust).
    QPolygonF tz;
    tz << QPointF(g.left() + 4, cy - r * 0.4) << QPointF(g.right() - 4, cy - r)
       << QPointF(g.right() - 4, cy + r) << QPointF(g.left() + 4, cy + r * 0.4);
    p->drawPolygon(tz);

  } else if (type == "Condenser") {
    // Box shell with a cooling serpentine.
    QRectF shell(g.left() + 2, cy - r, g.width() - 4, 2 * r);
    p->drawRect(shell);
    QPainterPath path;
    double x0 = shell.left() + 6, x1 = shell.right() - 6;
    path.moveTo(x0, cy);
    for (int i = 0; i <= 4; ++i)
      path.lineTo(x0 + (x1 - x0) * i / 4, cy + ((i % 2 == 0) ? -r * 0.5 : r * 0.5));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);

  } else if (type == "Deaerator") {
    // Horizontal storage vessel with a small dome (vent) on top.
    QRectF body(g.left() + 2, cy - r * 0.6, g.width() - 4, r * 1.2);
    p->drawRoundedRect(body, r * 0.5, r * 0.5);
    p->drawChord(QRectF(cx - r * 0.4, body.top() - r * 0.5, r * 0.8, r * 0.6), 0,
                 180 * 16);

  } else if (type == "Junction" || type == "Header") {
    if (type == "Header") {
      // Manifold: long horizontal cylinder.
      QRectF body(g.left() + 2, cy - r * 0.5, g.width() - 4, r);
      p->drawRoundedRect(body, r * 0.4, r * 0.4);
    } else {
      p->setBrush(QColor(40, 40, 40));
      p->drawEllipse(QPointF(cx, cy), 5, 5);
    }

  } else if (type == "Filter" || type == "Strainer") {
    // Diamond with a mesh hatch.
    QPolygonF dia;
    dia << QPointF(cx, cy - r) << QPointF(cx + r, cy) << QPointF(cx, cy + r)
        << QPointF(cx - r, cy);
    p->drawPolygon(dia);
    p->drawLine(QPointF(cx - r * 0.5, cy), QPointF(cx + r * 0.5, cy));
    p->drawLine(QPointF(cx, cy - r * 0.5), QPointF(cx, cy + r * 0.5));

  } else if (type == "Transformer") {
    // Two overlapping windings.
    double rr = r * 0.62;
    p->drawEllipse(QPointF(cx, cy - rr * 0.5), rr, rr);
    p->drawEllipse(QPointF(cx, cy + rr * 0.5), rr, rr);

  } else if (type == "Busbar") {
    // Thick horizontal bar.
    QPen bar(QColor(40, 40, 40));
    bar.setWidth(5);
    p->setPen(bar);
    p->drawLine(QPointF(g.left() + 2, cy), QPointF(g.right() - 2, cy));

  } else if (type == "Breaker") {
    // Switch contact in a square.
    p->drawRect(QRectF(cx - r, cy - r, 2 * r, 2 * r));
    p->drawLine(QPointF(cx - r, cy), QPointF(cx - r * 0.2, cy));
    p->drawLine(QPointF(cx - r * 0.2, cy), QPointF(cx + r * 0.6, cy - r * 0.7));
    p->drawLine(QPointF(cx + r * 0.2, cy), QPointF(cx + r, cy));

  } else if (type == "Actuator" || type == "PneumaticActuatorModulating" ||
             type == "PneumaticActuatorOnOff") {
    // Pneumatic diaphragm actuator: dome on a short stem.
    QRectF dome(cx - r * 0.8, g.top() + 2, r * 1.6, r);
    p->drawChord(dome, 0, 180 * 16);
    p->drawLine(QPointF(cx, dome.bottom()), QPointF(cx, g.bottom() - 2));
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    // Modulating carries a positioner "P"; on/off a solenoid "S".
    if (type == "PneumaticActuatorModulating")
      p->drawText(dome, Qt::AlignCenter, "P");
    else if (type == "PneumaticActuatorOnOff")
      p->drawText(dome, Qt::AlignCenter, "S");

  } else if (type == "ElectricActuator") {
    // Motor-operated actuator: a small motor circle on a stem.
    double mr = r * 0.55;
    p->drawEllipse(QPointF(cx, g.top() + 2 + mr), mr, mr);
    p->drawLine(QPointF(cx, g.top() + 2 + 2 * mr), QPointF(cx, g.bottom() - 2));
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->drawText(QRectF(cx - mr, g.top() + 2, 2 * mr, 2 * mr), Qt::AlignCenter, "M");

  } else if (type == "ManualActuator") {
    // Handwheel: a flat ellipse with a hub on a stem.
    QRectF wheel(cx - r * 0.9, g.top() + 4, r * 1.8, r * 0.5);
    p->drawEllipse(wheel);
    p->drawLine(QPointF(cx, wheel.center().y()), QPointF(cx, g.bottom() - 2));

  } else if (c.domain == Domain::Electrical || c.domain == Domain::Instrument ||
             c.domain == Domain::Control) {
    // Generic ISA-style bubble (Grid/Generator/Motor/Load, Transmitter, Switch,
    // RTD, Gauge, Controller, Timer, Logic, ...) with a short type code inside.
    p->drawEllipse(QPointF(cx, cy), r, r);
    if (c.domain == Domain::Instrument || c.domain == Domain::Control)
      p->drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));  // field mount line
    static const std::pair<const char*, const char*> codes[] = {
        {"Grid", "~"},   {"Generator", "G"},    {"Motor", "M"},
        {"ElectricalLoad", "L"}, {"Cable", "—"}, {"Transmitter", "T"},
        {"Switch", "S"}, {"RTD", "TE"},         {"Gauge", "I"},
        {"Controller", "PID"}, {"Timer", "TMR"}, {"Logic", "&"}};
    QString code = QString::fromStdString(type.substr(0, 1));
    for (auto& kv : codes)
      if (type == kv.first) { code = kv.second; break; }
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->drawText(QRectF(cx - r, cy - r * 0.5, 2 * r, r), Qt::AlignCenter, code);

  } else {
    // Fallback: rounded rectangle.
    p->drawRoundedRect(g.adjusted(2, 2, -2, -2), 6, 6);
  }
}
}  // namespace

ComponentItem::ComponentItem(Component* c) : comp_(c) {
  setFlag(ItemIsMovable, true);
  setFlag(ItemIsSelectable, true);
  setFlag(ItemSendsGeometryChanges, true);
  // The air receiver carries up to 20 nozzles, so it needs a wide footprint.
  if (c->type == "AirReceiver") w_ = 440.0;
  setPos(c->x, c->y);
  setZValue(1);
  layoutPorts();
}

void ComponentItem::layoutPorts() {
  ports_.clear();
  const double gh = glyphH_;
  std::vector<int> inlets, outlets, bidir;
  for (int i = 0; i < (int)comp_->ports.size(); ++i) {
    switch (comp_->ports[i].role) {
      case PortRole::Inlet: inlets.push_back(i); break;
      case PortRole::Outlet: outlets.push_back(i); break;
      case PortRole::Bidirectional: bidir.push_back(i); break;
    }
  }
  // Gas/steam bidirectional tappings (e.g. tank cover gas) go on the top edge;
  // other bidirectional ports go along the bottom. The air receiver's many
  // nozzles are split evenly between the top and bottom edges of its drum.
  std::vector<int> topPorts, bottomPorts;
  if (comp_->type == "AirReceiver") {
    for (size_t k = 0; k < bidir.size(); ++k)
      (k % 2 ? topPorts : bottomPorts).push_back(bidir[k]);
  } else {
    for (int i : bidir) {
      const Port& p = comp_->ports[i];
      if (p.name == "gas" || p.medium == Medium::Gas || p.medium == Medium::Steam)
        topPorts.push_back(i);
      else
        bottomPorts.push_back(i);
    }
  }
  auto placeSide = [&](const std::vector<int>& idxs, double xLocal) {
    int n = (int)idxs.size();
    for (int k = 0; k < n; ++k) {
      const Port& port = comp_->ports[idxs[k]];
      ports_.push_back({port.name, port.role,
                        QPointF(xLocal, gh * (k + 1.0) / (n + 1.0))});
    }
  };
  auto placeEdge = [&](const std::vector<int>& idxs, double yLocal) {
    int n = (int)idxs.size();
    for (int k = 0; k < n; ++k) {
      const Port& port = comp_->ports[idxs[k]];
      ports_.push_back({port.name, port.role,
                        QPointF(w_ * (k + 1.0) / (n + 1.0), yLocal)});
    }
  };
  placeSide(inlets, 0.0);
  placeSide(outlets, w_);
  placeEdge(topPorts, 0.0);
  placeEdge(bottomPorts, gh);
}

QRectF ComponentItem::boundingRect() const {
  return QRectF(-10, -10, w_ + 20, h_ + 24);
}

void ComponentItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
  // Selection halo.
  if (isSelected()) {
    QPen halo(QColor(20, 90, 200));
    halo.setWidth(2);
    halo.setStyle(Qt::DashLine);
    p->setPen(halo);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(-4, -4, w_ + 8, h_ + 8), 6, 6);
  }

  // Symbol glyph in the upper region.
  QRectF glyph(0, 0, w_, glyphH_);
  drawSymbol(p, comp_->type, *comp_, glyph);

  // Tag (P&ID name) + type/fluid caption below the glyph.
  p->setPen(QColor(20, 20, 20));
  QFont f = p->font();
  f.setBold(true);
  f.setPointSizeF(f.pointSizeF() + 0.5);
  p->setFont(f);
  QString tag = QString::fromStdString(comp_->name.empty() ? comp_->type
                                                           : comp_->name);
  p->drawText(QRectF(0, glyphH_, w_, 16), Qt::AlignHCenter | Qt::AlignTop, tag);

  f.setBold(false);
  f.setPointSizeF(f.pointSizeF() - 1.0);
  p->setFont(f);
  QString sub = QString::fromStdString(comp_->type);
  if (comp_->domain == Domain::Hydraulic)
    sub += " · " + QString::fromStdString(comp_->fluid);
  QString elided =
      p->fontMetrics().elidedText(sub, Qt::ElideRight, (int)w_ - 4);
  p->drawText(QRectF(0, glyphH_ + 15, w_, 14), Qt::AlignHCenter | Qt::AlignTop,
              elided);

  // Live runtime readout (during a simulation run).
  if (!runtime_.isEmpty()) {
    p->setPen(QColor(20, 110, 40));
    QFont mf = p->font();
    mf.setPointSizeF(mf.pointSizeF() - 0.5);
    p->setFont(mf);
    p->drawText(QRectF(-6, glyphH_ + 29, w_ + 12, 28),
                Qt::AlignHCenter | Qt::AlignTop, runtime_);
  }

  // Port handles.
  for (const auto& pv : ports_) {
    p->setBrush(portColor(pv.role));
    p->setPen(QPen(Qt::black, 1));
    p->drawEllipse(pv.local, 5, 5);
  }
}

void ComponentItem::setRuntime(const QString& s) {
  if (runtime_ == s) return;
  runtime_ = s;
  update();
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
  if (idx < 0 || idx >= (int)ports_.size())
    return mapToScene(QPointF(w_ / 2, glyphH_ / 2));
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
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  updatePosition();
}

void ConnectionItem::updatePosition() {
  setLine(QLineF(a_->portScenePos(pa_), b_->portScenePos(pb_)));
}

void ConnectionItem::setFlow(double signedFlow) {
  flow_ = signedFlow;
  hasFlow_ = true;
  update();
}

void ConnectionItem::paint(QPainter* p, const QStyleOptionGraphicsItem* o,
                           QWidget* w) {
  if (isSelected()) {
    QPen hl(QColor(20, 90, 200));
    hl.setWidth(4);
    p->setPen(hl);
    p->drawLine(line());
  }
  QGraphicsLineItem::paint(p, o, w);
  if (!hasFlow_ || std::fabs(flow_) < 1e-9) return;

  // Draw a direction arrowhead at the line midpoint. The arrow points from A to
  // B when flow_ > 0, and from B to A when flow_ < 0.
  QLineF ln = line();
  QPointF mid = (ln.p1() + ln.p2()) / 2.0;
  QPointF dir = (flow_ >= 0.0) ? (ln.p2() - ln.p1()) : (ln.p1() - ln.p2());
  double len = std::hypot(dir.x(), dir.y());
  if (len < 1e-6) return;
  dir /= len;
  QPointF norm(-dir.y(), dir.x());
  double a = 7.0;  // arrow size
  QPolygonF head;
  head << mid + dir * a << mid - dir * a + norm * a * 0.7
       << mid - dir * a - norm * a * 0.7;
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setBrush(QColor(30, 110, 200));
  p->setPen(Qt::NoPen);
  p->drawPolygon(head);
}

}  // namespace umpnap

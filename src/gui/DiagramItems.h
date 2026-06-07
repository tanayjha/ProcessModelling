#pragma once
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QString>
#include <string>
#include <vector>

#include "core/Component.h"

namespace umpnap {

// Visual + interactive representation of one Component on the canvas.
class ComponentItem : public QGraphicsItem {
 public:
  explicit ComponentItem(Component* c);

  Component* comp() const { return comp_; }

  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

  // Index of the port whose handle is under scenePos, or -1.
  int portAt(const QPointF& scenePos) const;
  QPointF portScenePos(int idx) const;
  int portIndexByName(const std::string& name) const;
  const std::string& portName(int idx) const;
  PortRole portRole(int idx) const;
  int portCount() const { return (int)ports_.size(); }

  // Live runtime readout shown under the symbol while a simulation runs.
  void setRuntime(const QString& s);

 protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

 private:
  struct PortVis {
    std::string name;
    PortRole role;
    QPointF local;
  };
  void layoutPorts();

  Component* comp_;
  std::vector<PortVis> ports_;
  QString runtime_;        // live value text (empty when idle)
  double w_ = 104.0;       // footprint width
  double h_ = 92.0;        // footprint height (glyph + caption)
  double glyphH_ = 58.0;   // height of the symbol glyph region
};

// A wire between two component ports, with an optional animated flow arrow.
class ConnectionItem : public QGraphicsLineItem {
 public:
  ConnectionItem(ComponentItem* a, int pa, ComponentItem* b, int pb);
  void updatePosition();
  // signedFlow > 0 means flow runs from the A endpoint toward the B endpoint.
  void setFlow(double signedFlow);
  void paint(QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w) override;

 private:
  ComponentItem* a_;
  int pa_;
  ComponentItem* b_;
  int pb_;
  double flow_ = 0.0;
  bool hasFlow_ = false;
};

}  // namespace umpnap

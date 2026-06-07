#pragma once
#include <QGraphicsItem>
#include <QGraphicsLineItem>
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
  double w_ = 104.0;       // footprint width
  double h_ = 92.0;        // footprint height (glyph + caption)
  double glyphH_ = 58.0;   // height of the symbol glyph region
};

// A wire between two component ports.
class ConnectionItem : public QGraphicsLineItem {
 public:
  ConnectionItem(ComponentItem* a, int pa, ComponentItem* b, int pb);
  void updatePosition();

 private:
  ComponentItem* a_;
  int pa_;
  ComponentItem* b_;
  int pb_;
};

}  // namespace umpnap

#pragma once
#include <QGraphicsScene>
#include <QString>
#include <vector>

#include "core/Network.h"

class QGraphicsLineItem;

namespace umpnap {

class ComponentItem;
class ConnectionItem;
class Results;

// Interactive P&ID scene: place from palette, drag to move, port-drag to wire.
class DiagramScene : public QGraphicsScene {
  Q_OBJECT
 public:
  explicit DiagramScene(Network* net, QObject* parent = nullptr);

  void setArmedType(const QString& type) { armedType_ = type; }
  void rebuildFromNetwork();   // recreate all items from net_
  void refreshConnections();   // reposition all wires
  void selectComponent(int id);

  // Runtime monitoring: push live values onto symbols and flow arrows onto wires.
  void updateRuntime(const Results& res);
  void clearRuntime();

 signals:
  void componentSelected(umpnap::Component* c);  // nullptr when cleared
  void networkChanged();

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;

 private:
  ComponentItem* itemForId(int id) const;
  ComponentItem* portHitTest(const QPointF& scenePos, int& portIdx) const;
  void addComponentItem(Component* c);
  void addConnectionItemForIndex(int connIndex);
  void onSelectionChanged();

  Network* net_;
  QString armedType_;
  std::vector<ComponentItem*> items_;
  std::vector<ConnectionItem*> conns_;

  bool connecting_ = false;
  ComponentItem* srcItem_ = nullptr;
  int srcPort_ = -1;
  QGraphicsLineItem* tempLine_ = nullptr;
};

}  // namespace umpnap

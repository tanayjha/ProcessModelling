#pragma once
#include <QGraphicsScene>
#include <QPointF>
#include <QString>
#include <map>
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
  // Editing (delete of components/wires) is blocked while the simulation runs;
  // re-enabled when it is paused or stopped.
  void setEditable(bool on) { editable_ = on; }
  void rebuildFromNetwork();   // recreate all items from net_
  void refreshConnections();   // reposition all wires
  void selectComponent(int id);

  // Runtime monitoring: push live values onto symbols and flow arrows onto wires.
  void updateRuntime(const Results& res);
  void clearRuntime();

 signals:
  void componentSelected(umpnap::Component* c);  // nullptr when cleared
  void networkChanged();
  void connectionRejected(const QString& reason);

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

  bool editable_ = true;
  // Component positions captured on mouse-press, so a drag is detected on
  // release and reported via networkChanged() (for undo capture).
  std::map<int, QPointF> movePressPos_;
};

}  // namespace umpnap

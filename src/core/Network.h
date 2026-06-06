#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/Component.h"

namespace umpnap {

struct Connection {
  int compA = -1;
  std::string portA;
  int compB = -1;
  std::string portB;
};

class Network {
 public:
  // Takes ownership; assigns the component a fresh id and returns it.
  int addComponent(std::unique_ptr<Component> c);
  // Removes the component and any connections that reference it.
  void removeComponent(int id);
  Component* component(int id);
  const Component* component(int id) const;
  const std::vector<std::unique_ptr<Component>>& components() const { return comps_; }

  void connect(int a, const std::string& pa, int b, const std::string& pb);
  void disconnect(int index);
  const std::vector<Connection>& connections() const { return conns_; }

  // Returns "" if valid, else the first problem found.
  std::string validate() const;

  void clear();
  int nextId() const { return nextId_; }
  void setNextId(int v) { nextId_ = v; }

 private:
  std::vector<std::unique_ptr<Component>> comps_;
  std::vector<Connection> conns_;
  int nextId_ = 1;
};

}  // namespace umpnap

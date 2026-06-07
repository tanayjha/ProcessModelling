#include "core/Project.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/ComponentRegistry.h"

namespace umpnap {

// ---------------- Minimal JSON value + parser (self-contained) -------------
namespace {

struct JValue {
  enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
  bool b = false;
  double num = 0.0;
  std::string str;
  std::vector<JValue> arr;
  std::map<std::string, JValue> obj;

  const JValue* get(const std::string& k) const {
    auto it = obj.find(k);
    return it == obj.end() ? nullptr : &it->second;
  }
  double numOr(const std::string& k, double d) const {
    const JValue* v = get(k);
    return v && v->type == Num ? v->num : d;
  }
  std::string strOr(const std::string& k, const std::string& d) const {
    const JValue* v = get(k);
    return v && v->type == Str ? v->str : d;
  }
};

struct Parser {
  const std::string& s;
  size_t i = 0;
  bool ok = true;
  explicit Parser(const std::string& src) : s(src) {}

  void ws() {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r'))
      ++i;
  }
  JValue parse() {
    ws();
    return value();
  }
  JValue value() {
    ws();
    if (i >= s.size()) { ok = false; return {}; }
    char c = s[i];
    if (c == '{') return object();
    if (c == '[') return array();
    if (c == '"') { JValue v; v.type = JValue::Str; v.str = str(); return v; }
    if (c == 't' || c == 'f') return boolean();
    if (c == 'n') { i += 4; JValue v; v.type = JValue::Null; return v; }
    return number();
  }
  std::string str() {
    std::string out;
    ++i;  // opening quote
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c == '\\' && i < s.size()) {
        char e = s[i++];
        switch (e) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          default: out += e; break;
        }
      } else {
        out += c;
      }
    }
    if (i < s.size()) ++i;  // closing quote
    return out;
  }
  JValue number() {
    size_t start = i;
    while (i < s.size() &&
           (isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '+' ||
            s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
      ++i;
    JValue v;
    v.type = JValue::Num;
    v.num = std::strtod(s.substr(start, i - start).c_str(), nullptr);
    return v;
  }
  JValue boolean() {
    JValue v;
    v.type = JValue::Bool;
    if (s.compare(i, 4, "true") == 0) { v.b = true; i += 4; }
    else { v.b = false; i += 5; }
    return v;
  }
  JValue array() {
    JValue v;
    v.type = JValue::Arr;
    ++i;  // [
    ws();
    if (i < s.size() && s[i] == ']') { ++i; return v; }
    while (i < s.size()) {
      v.arr.push_back(value());
      ws();
      if (i < s.size() && s[i] == ',') { ++i; continue; }
      if (i < s.size() && s[i] == ']') { ++i; break; }
      break;
    }
    return v;
  }
  JValue object() {
    JValue v;
    v.type = JValue::Obj;
    ++i;  // {
    ws();
    if (i < s.size() && s[i] == '}') { ++i; return v; }
    while (i < s.size()) {
      ws();
      std::string key = str();
      ws();
      if (i < s.size() && s[i] == ':') ++i;
      v.obj[key] = value();
      ws();
      if (i < s.size() && s[i] == ',') { ++i; continue; }
      if (i < s.size() && s[i] == '}') { ++i; break; }
      break;
    }
    return v;
  }
};

std::string esc(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

}  // namespace

bool saveProject(const Network& net, const std::string& path) {
  std::ofstream f(path);
  if (!f) return false;
  f << "{\n";
  f << "  \"nextId\": " << net.nextId() << ",\n";
  f << "  \"components\": [\n";
  const auto& comps = net.components();
  for (size_t ci = 0; ci < comps.size(); ++ci) {
    const Component& c = *comps[ci];
    f << "    {\"id\": " << c.id << ", \"type\": \"" << esc(c.type)
      << "\", \"name\": \"" << esc(c.name) << "\", \"domain\": \""
      << domainName(c.domain) << "\", \"x\": " << c.x << ", \"y\": " << c.y
      << ", \"fluid\": \"" << esc(c.fluid) << "\", \"params\": {";
    bool first = true;
    for (const auto& kv : c.params) {
      if (!first) f << ", ";
      first = false;
      f << "\"" << esc(kv.first) << "\": " << kv.second;
    }
    f << "}";
    // Optional curve data (e.g. pump head curve) as arrays of [x, y] pairs.
    if (!c.curves.empty()) {
      f << ", \"curves\": {";
      bool firstCurve = true;
      for (const auto& cv : c.curves) {
        if (!firstCurve) f << ", ";
        firstCurve = false;
        f << "\"" << esc(cv.first) << "\": [";
        for (size_t i = 0; i < cv.second.size(); ++i) {
          if (i) f << ", ";
          f << "[" << cv.second[i].first << ", " << cv.second[i].second << "]";
        }
        f << "]";
      }
      f << "}";
    }
    f << "}";
    if (ci + 1 < comps.size()) f << ",";
    f << "\n";
  }
  f << "  ],\n";
  f << "  \"connections\": [\n";
  const auto& conns = net.connections();
  for (size_t i = 0; i < conns.size(); ++i) {
    const Connection& cn = conns[i];
    f << "    {\"compA\": " << cn.compA << ", \"portA\": \"" << esc(cn.portA)
      << "\", \"compB\": " << cn.compB << ", \"portB\": \"" << esc(cn.portB)
      << "\"}";
    if (i + 1 < conns.size()) f << ",";
    f << "\n";
  }
  f << "  ]\n}\n";
  return true;
}

bool loadProject(Network& net, const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::stringstream ss;
  ss << f.rdbuf();
  std::string text = ss.str();

  Parser p(text);
  JValue root = p.parse();
  if (root.type != JValue::Obj) return false;

  registerHydraulicComponents();
  auto& reg = ComponentRegistry::instance();
  net.clear();

  const JValue* comps = root.get("components");
  int maxId = 0;
  // addComponent assigns the id from nextId; set nextId to the stored id first
  // so original ids (referenced by connections) are preserved.
  if (comps && comps->type == JValue::Arr) {
    for (const auto& cj : comps->arr) {
      std::string type = cj.strOr("type", "");
      std::unique_ptr<Component> c = reg.create(type);
      if (!c) {
        c = std::make_unique<Component>();
        c->type = type;
      }
      int id = (int)cj.numOr("id", 0);
      c->name = cj.strOr("name", "");
      c->domain = domainFromName(cj.strOr("domain", "Hydraulic"));
      c->x = cj.numOr("x", 0);
      c->y = cj.numOr("y", 0);
      c->fluid = cj.strOr("fluid", "Light Water");
      const JValue* params = cj.get("params");
      if (params && params->type == JValue::Obj)
        for (const auto& kv : params->obj)
          if (kv.second.type == JValue::Num) c->params[kv.first] = kv.second.num;
      // Curve data: { "head": [[Q,H], ...], ... }
      const JValue* curves = cj.get("curves");
      if (curves && curves->type == JValue::Obj) {
        for (const auto& cv : curves->obj) {
          if (cv.second.type != JValue::Arr) continue;
          std::vector<std::pair<double, double>> pts;
          for (const auto& pair : cv.second.arr) {
            if (pair.type == JValue::Arr && pair.arr.size() >= 2 &&
                pair.arr[0].type == JValue::Num && pair.arr[1].type == JValue::Num)
              pts.push_back({pair.arr[0].num, pair.arr[1].num});
          }
          c->curves[cv.first] = std::move(pts);
        }
      }
      net.setNextId(id);
      net.addComponent(std::move(c));  // assigns id == id, bumps nextId
      if (id > maxId) maxId = id;
    }
  }
  net.setNextId(maxId + 1);

  const JValue* conns = root.get("connections");
  if (conns && conns->type == JValue::Arr) {
    for (const auto& cn : conns->arr) {
      net.connect((int)cn.numOr("compA", 0), cn.strOr("portA", ""),
                  (int)cn.numOr("compB", 0), cn.strOr("portB", ""));
    }
  }
  return true;
}

}  // namespace umpnap

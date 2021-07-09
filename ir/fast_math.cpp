// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/fast_math.h"
#include "ir/attrs.h"
#include "util/unionfind.h"
#include <map>

#define DEBUG_FMF

#ifdef DEBUG_FMF
# include <iostream>
#endif

using namespace IR;
using namespace smt;
using namespace util;
using namespace std;

// Home-made e-graph to represent all possible fast-math reassociations

namespace {

class EGraph;

struct Node {
  enum { Add, Sub, Mul, Div, Neg, Leaf } operation;
  unsigned op1 = 0, op2 = 0;
  expr leaf;
  // FIXME: we currently ignore rounding
  expr rounding;
  unsigned flags = 0;

#if __cplusplus > 201703L
  auto operator<=>(const Node &other) const = default;
#else
  bool operator==(const Node& rhs) const {
      return this->operation == rhs.operation
          && this->op1 == rhs.op1
          && this->op2 == rhs.op2
          && this->leaf == rhs.leaf
          && this->rounding == rhs.rounding
          && this->flags == rhs.flags;
  }
  bool operator!=(const Node& rhs) const {
      return !(*this == rhs);
  }
  bool operator<(const Node& rhs)  const {
    if (this->operation < rhs.operation) {
      return true;
    }
    if (this->op1 < rhs.op1) {
      return true;
    }
    if (this->op2 < rhs.op2) {
      return true;
    }
    if (this->leaf < rhs.leaf) {
      return true;
    }
    if (this->rounding < rhs.rounding) {
      return true;
    }
    return (this->flags < rhs.flags);
  }
#endif
  
#ifdef DEBUG_FMF
  void print(ostream &os, EGraph &g) const;
#endif
};


class EGraph {
  UnionFind uf;
  map<Node, unsigned> nodes;

  unsigned get(Node &&n) {
    typename decltype(nodes)::iterator I;
    bool inserted;
    std::tie(I, inserted) = util::map_try_emplace(nodes, move(n), 0);
    if (inserted)
      I->second = uf.mk();
    return I->second;
  }

  void decl_equivalent(Node &&node, unsigned n1) {
    unsigned n2 = util::map_try_emplace(nodes, move(node), n1).first->second;
    uf.merge(n1, n2);
  }

public:
  unsigned root(unsigned n) {
    return uf.find(n);
  }

  unsigned get(const expr &e0) {
    Node n;
    expr rounding, a, b;
    bool is_leaf = false;

    expr e = e0;

    while (true) {
      if (e.isFPAdd(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Add;
      } else if (e.isFPSub(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Sub;
      } else if (e.isFPMul(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Mul;
      } else if (e.isFPDiv(rounding, a, b)) {
        n.op2 = get(b);
        n.operation = Node::Div;
      } else if (e.isFPNeg(a)) {
        n.operation = Node::Neg;
      } else if (!e.fn_name().empty()) {
        if (e.fn_name() == "reassoc") {
          n.flags |= FastMathFlags::Reassoc;
          e = e.getFnArg(0);
          continue;
        }
        is_leaf = true;
      } else {
        is_leaf = true;
      }

      if (is_leaf) {
        n.operation = Node::Leaf;
        n.leaf      = move(e);
      } else {
        n.op1      = get(a);
        n.rounding = move(rounding);
      }

      return get(move(n));
    }
  }

  void saturate() {
    while (true) {
      auto nodes_copy = nodes;
      for (auto &node_val : nodes_copy) {
        auto &node = std::get<0>(node_val);
        auto &n = std::get<1>(node_val);
        // commutativity: x . y == y . x
        // Always correct for add & mul regardless of fast-math
        if (node.operation == Node::Add || node.operation == Node::Mul) {
          Node nn = node;
          swap(nn.op1, nn.op2);
          decl_equivalent(move(nn), n);
        }

        if (node.flags & FastMathFlags::Reassoc) {
          // distributivity: x . (y + z) == x . y + x . z

          // associativity (x . y) . z == x . (y . z)

        }

        if (node.flags & FastMathFlags::Contract) {
          // x / y == x * (1 / y)
          // TODO
        }
      }
      if (nodes.size() == nodes_copy.size())
        return;
    }
  }

#ifdef DEBUG_FMF
  friend ostream& operator<<(ostream &os, EGraph &g) {
    vector<vector<const Node*>> sorted;
    for (auto &node : g.nodes) {
      unsigned r = g.root(node.second);
      if (r >= sorted.size())
        sorted.resize(r + 1);
      sorted[r].emplace_back(&node.first);
    }

    for (unsigned i = 0, e = sorted.size(); i != e; ++i) {
      auto &nodes = sorted[i];
      if (!nodes.empty()) {
        os << "Root " << i << '\n';
        for (auto *n : nodes) {
          n->print(os << "  ", g);
          os << '\n';
        }
      }
    }
    return os;
  }
#endif
};


#ifdef DEBUG_FMF
void Node::print(ostream &os, EGraph &g) const {
  const char *op;
  switch (operation) {
    case Add:  op = "add "; break;
    case Sub:  op = "sub "; break;
    case Mul:  op = "mul "; break;
    case Div:  op = "div "; break;
    case Neg:  op = "neg "; break;
    case Leaf: op = "leaf "; break;
  }
  os << op;

  if (operation == Leaf) {
    os << leaf;
    return;
  }

  os << g.root(op1);
  if (operation != Neg) {
    os << ", " << g.root(op2);
  }

  FastMathFlags fmf;
  fmf.flags = flags;
  if (!fmf.isNone())
    os << "\t# " << fmf;
}
#endif

}

namespace IR {

expr float_refined(const expr &a, const expr &b) {
  EGraph g;
  unsigned na = g.get(a);
  unsigned nb = g.get(b);
#ifdef DEBUG_FMF
  cout << "Before saturate:\n" << g << "Roots: " << na << " / " << nb << "\n\n";
#endif

  if (na == nb)
    return true;

  g.saturate();

  na = g.root(na);
  nb = g.root(nb);
#ifdef DEBUG_FMF
  cout << "After saturate:\n" << g << "Roots: " << na << " / " << nb << "\n\n";
#endif
  if (na == nb)
    return true;

  // TODO
  return false;
}

}

#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include "util/compat.h"
#include "util/compiler.h"
#include "util/optional.h"
#include <cassert>
#include <compare>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <tuple>
#include <utility>

namespace smt {

class AndExpr {
  std::set<expr> exprs;

public:
  AndExpr() {}
  template <typename T>
  AndExpr(T &&e) { add(std::forward<T>(e)); }

  void add(const expr &e);
  void add(expr &&e);
  void add(const AndExpr &other);
  void del(const AndExpr &other);
  void reset();
  bool contains(const expr &e) const;
  expr operator()() const;
  operator bool() const;
  bool isTrue() const { return exprs.empty(); }
  friend std::ostream &operator<<(std::ostream &os, const AndExpr &e);
};


class OrExpr {
  std::set<expr> exprs;

public:
  void add(expr &&e);
  void add(const OrExpr &other);
  expr operator()() const;
  friend std::ostream &operator<<(std::ostream &os, const OrExpr &e);
};


template <typename T>
class DisjointExpr {
  std::map<T, expr> vals; // val -> domain
  util::optional<T> default_val;

public:
  DisjointExpr() {}
  DisjointExpr(const T &default_val) : default_val(default_val) {}
  DisjointExpr(const util::optional<T> &default_val) : default_val(default_val){}
  DisjointExpr(T &&default_val) : default_val(std::move(default_val)) {}
  DisjointExpr(const expr &e, unsigned depth_limit);

  template <typename V, typename D>
  void add(V &&val, D &&domain) {
    if (domain.isFalse())
      return;
    if (domain.isTrue())
      vals.clear();

    typename decltype(vals)::iterator I;
    bool inserted;
    std::tie(I, inserted) = util::map_try_emplace(vals, std::forward<V>(val), std::forward<D>(domain));

    if (!inserted)
      I->second |= std::forward<D>(domain);
  }

  template <typename D>
  void add_disj(const DisjointExpr<T> &other, D &&domain) {
    assert(!default_val && !other.default_val);
    for (auto &t : other.vals) {
      add(t.first, t.second && std::forward<D>(domain));
    }
  }

  util::optional<T> operator()() const {
    util::optional<T> ret;
    for (auto &t : vals) {
      expr domain;
      std::tie(std::ignore, domain) = t;
      if (domain.isTrue())
        return t.first;

      ret = ret ? T::mkIf(domain, t.first, *ret) : t.first;
    }
    return ret ? ret : default_val;
  }

  util::optional<T> lookup(const expr &domain) const {
    for (auto &t : vals) {
      if (t.second.eq(domain))
        return t.first;
    }
    return {};
  }

  auto begin() const { return vals.begin(); }
  auto end() const   { return vals.end(); }
  auto size() const  { return vals.size(); }
};


// non-deterministic choice of one of the options with potentially overlapping
// domains
template <typename T>
class ChoiceExpr {
  std::map<T, expr> vals; // val -> domain

public:
  template <typename V, typename D>
  void add(V &&val, D &&domain) {
    if (domain.isFalse())
      return;
    typename decltype(vals)::iterator I;
    bool inserted;
    std::tie(I, inserted) = util::map_try_emplace(vals,
                                                  std::forward<V>(val),
                                                  std::forward<D>(domain));
    if (!inserted)
      I->second |= std::forward<D>(domain);
  }

  operator bool() const {
    return !vals.empty();
  }

  expr domain() const {
    OrExpr ret;
    for (auto &p : vals) {
      ret.add(expr(p.second));
    }
    return ret();
  }

  // returns: data, domain, quant var, precondition
  std::tuple<T,expr,expr,expr> operator()() const {
    if (vals.size() == 1)
      return { vals.begin()->first, vals.begin()->second, expr(), true };

    expr dom = domain();
    unsigned bits = util::ilog2_ceil(vals.size()+1, false);
    expr qvar = expr::mkFreshVar("choice", expr::mkUInt(0, bits));

    T ret;
    expr pre = !dom;
    auto I = vals.begin();
    bool first = true;

    for (unsigned i = vals.size(); i > 0; --i) {
      auto cmp = qvar == (i-1);
      pre = expr::mkIf(cmp, I->second, pre);
      ret = first ? I->first : T::mkIf(cmp, I->first, ret);
      first = false;
      ++I;
    }

    return { std::move(ret), std::move(dom), std::move(qvar), std::move(pre) };
  }
};


class FunctionExpr {
  std::map<expr, expr> fn; // key -> val
  util::optional<expr> default_val;

public:
  FunctionExpr() {}
  FunctionExpr(expr &&default_val) : default_val(std::move(default_val)) {}
  void add(const expr &key, expr &&val);
  void add(const FunctionExpr &other);
  void del(const expr &key);

  util::optional<expr> operator()(const expr &key) const;
  const expr* lookup(const expr &key) const;

  FunctionExpr simplify() const;

  auto begin() const { return fn.begin(); }
  auto end() const { return fn.end(); }
  bool empty() const { return fn.empty() && !default_val; }

#if __cplusplus > 201703L
  std::weak_ordering operator<=>(const FunctionExpr &rhs) const;
#else
  bool operator==(const FunctionExpr& rhs) const {
      return this->fn == rhs.fn
          && this->default_val == rhs.default_val;
  }
  bool operator!=(const FunctionExpr& rhs) const {
      return !(*this == rhs);
  }
  bool operator<(const FunctionExpr& rhs) const {
    if (this->fn < rhs.fn) {
      return true;
    }
    return this->default_val < rhs.default_val;
  }
#endif
  friend std::ostream& operator<<(std::ostream &os, const FunctionExpr &e);
};

}

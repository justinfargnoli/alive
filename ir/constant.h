#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "util/optional.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace IR {

class Constant : public Value {
public:
  Constant(Type &type, std::string &&name) : Value(type, std::move(name)) {}
  void print(std::ostream &os) const override;
};


class IntConst final : public Constant {
  util::optional<int64_t> i;
  util::optional<std::string> s;

public:
  IntConst(Type &type, int64_t val);
  IntConst(Type &type, std::string &&val);
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  const int64_t *getInt() const { return i.getPointer(); }
};


class FloatConst final : public Constant {
  util::optional<double> d;
  util::optional<int64_t> i;
  util::optional<std::string> s;

public:
  FloatConst(Type &type, double val);
  FloatConst(Type &type, uint64_t val);
  FloatConst(Type &type, std::string val);

  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class ConstantInput final : public Constant {
public:
  ConstantInput(Type &type, std::string &&name)
    : Constant(type, std::move(name)) {}
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class ConstantBinOp final : public Constant {
public:
  enum Op { ADD, SUB, SDIV, UDIV };

private:
  Constant &lhs, &rhs;
  Op op;

public:
  ConstantBinOp(Type &type, Constant &lhs, Constant &rhs, Op op);
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class ConstantFn final : public Constant {
  enum Fn { LOG2, WIDTH } fn;
  std::vector<Value*> args;

public:
  ConstantFn(Type &type, std::string_view name, std::vector<Value*> &&args);
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};

struct ConstantFnException {
  std::string str;
  ConstantFnException(std::string &&str) : str(std::move(str)) {}
};

util::optional<int64_t> getInt(const IR::Value &val);
uint64_t getIntOr(const IR::Value &val, uint64_t default_value);
}

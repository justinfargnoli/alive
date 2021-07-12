// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/constant.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include <cassert>

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

void Constant::print(ostream &os) const {
  UNREACHABLE();
}


IntConst::IntConst(Type &type, int64_t val)
  : Constant(type, to_string(val)), i(val), s() {}

IntConst::IntConst(Type &type, string &&val)
  : Constant(type, string(val)), i(), s(move(val)) {}

StateValue IntConst::toSMT(State &s) const {
  if (auto v = i.getPointer())
    return { expr::mkInt(*v, bits()), true };
  return { expr::mkInt(this->s.getPointer()->c_str(), bits()), true };
}

expr IntConst::getTypeConstraints() const {
  unsigned min_bits = 0;
  if (auto v = i.getPointer())
    min_bits = (*v >= 0 ? 63 : 64) - num_sign_bits(*v);

  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType().sizeVar().uge(min_bits);
}


FloatConst::FloatConst(Type &type, double val)
  : Constant(type, to_string(val)), d(val), i(), s() {}

FloatConst::FloatConst(Type &type, uint64_t val)
  : Constant(type, to_string(val)), d(), i(val), s() {}

FloatConst::FloatConst(Type &type, string val)
  : Constant(type, string(val)), d(), i(), s(move(val)) {}

expr FloatConst::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         getType().enforceFloatType();
}

StateValue FloatConst::toSMT(State &s) const {
  if (auto n = i.getPointer()) {
    return { expr::mkUInt(*n, getType().bits())
               .BV2float(getType().getDummyValue(true).value),
             true };
  }

  if (auto n = this->s.getPointer())
    return { expr::mkNumber(n->c_str(), getType().getDummyValue(true).value),
             true };

  expr e;
  double v = *d.getPointer();
  switch (getType().getAsFloatType()->getFpType()) {
  case FloatType::Half:    e = expr::mkHalf((float)v); break;
  case FloatType::Float:   e = expr::mkFloat((float)v); break;
  case FloatType::Double:  e = expr::mkDouble(v); break;
  case FloatType::Quad:
  case FloatType::Unknown: UNREACHABLE();
  }
  return { move(e), true };
}


StateValue ConstantInput::toSMT(State &s) const {
  auto type = getType().getDummyValue(false).value;
  return { expr::mkVar(getName().c_str(), type), true };
}

expr ConstantInput::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         (getType().enforceIntType() || getType().enforceFloatType());
}


ConstantBinOp::ConstantBinOp(Type &type, Constant &lhs, Constant &rhs, Op op)
  : Constant(type, ""), lhs(lhs), rhs(rhs), op(op) {
  const char *opname = nullptr;
  switch (op) {
  case ADD:  opname = " + "; break;
  case SUB:  opname = " - "; break;
  case SDIV: opname = " / "; break;
  case UDIV: opname = " /u "; break;
  }

  string str = '(' + this->lhs.getName();
  str += opname;
  str += rhs.getName();
  str += ')';
  this->setName(move(str));
}

static void div_ub(const expr &a, const expr &b, State &s, bool sign) {
  s.addPre(b != 0);
  if (sign)
    s.addPre(a.cmp_neq(expr::IntSMin(b.bits()), true)
             || b.cmp_neq(-1, true));
}

StateValue ConstantBinOp::toSMT(State &s) const {
  auto &va = s[lhs];
  auto &vb = s[rhs];
  expr val;

  switch (op) {
  case ADD: val = va.value + vb.value; break;
  case SUB: val = va.value - vb.value; break;
  case SDIV:
    val = va.value.sdiv(vb.value);
    div_ub(va.value, vb.value, s, true);
    break;
  case UDIV:
    val = va.value.udiv(vb.value);
    div_ub(va.value, vb.value, s, false);
    break;
  }
  return { move(val), va.non_poison && vb.non_poison };
}

expr ConstantBinOp::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType() == lhs.getType() &&
         getType() == rhs.getType();
}


ConstantFn::ConstantFn(Type &type, string_view name, vector<Value*> &&args)
  : Constant(type, ""), args(move(args)) {
  unsigned num_args;
  if (name == "log2") {
    fn = LOG2;
    num_args = 1;
  } else if (name == "width") {
    fn = WIDTH;
    num_args = 1;
  } else {
    throw ConstantFnException("Unknown function: " + string(name));
  }

  auto actual_args = this->args.size();
  if (actual_args != num_args)
    throw ConstantFnException("Expected " + to_string(num_args) +
                              " parameters for " + string(name) + ", but got " +
                              to_string(actual_args));

  string str = string(name) + '(';
  bool first = true;
  for (auto arg : this->args) {
    if (!first)
      str += ", ";
    first = false;
    str += arg->getName();
  }
  str += ')';
  this->setName(move(str));
}

StateValue ConstantFn::toSMT(State &s) const {
  expr r;
  switch (fn) {
  case LOG2: {
    auto &v_st = s[*args[0]];
    return { v_st.value.log2(bits()), expr(v_st.non_poison) };
  }
  case WIDTH:
    r = args[0]->bits();
    break;
  }
  return { move(r), true };
}

expr ConstantFn::getTypeConstraints() const {
  expr r = Value::getTypeConstraints();
  for (auto a : args) {
    r &= a->getTypeConstraints();
  }

  Type &ty = getType();
  switch (fn) {
  case LOG2:
  case WIDTH:
    r &= ty.enforceIntType();
    break;
  }
  return r;
}

optional<int64_t> getInt(const Value &val) {
  if (auto i = dynamic_cast<const IntConst*>(&val)) {
    if (auto n = i->getInt())
      return *n;
  }
  return {};
}

uint64_t getIntOr(const Value &val, uint64_t default_value) {
  if (auto n = getInt(val))
    return *n;
  return default_value;
}
}

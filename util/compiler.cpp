// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#include <algorithm>
#include <bit>

using namespace std;

namespace details {
bool has_single_bit(uint64_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}

unsigned bit_width(uint64_t n) {
  return 64 - __builtin_clz(n);
}
}

namespace util {

unsigned ilog2(uint64_t n) {
  return n == 0 ? 0 : details::bit_width(n) - 1;
}

unsigned ilog2_ceil(uint64_t n, bool up_power2) {
  auto log = ilog2(n);
  return !up_power2 && is_power2(n) ? log : log + 1;
}

bool is_power2(uint64_t n, uint64_t *log) {
  if (!__has_single_bit(n))
    return false;

  if (log)
    *log = ilog2(n);
  return true;
}

unsigned num_sign_bits(uint64_t n) {
  return max(__builtin_clz(n), __builtin_clz(~n)) -1;
}

uint64_t add_saturate(uint64_t a, uint64_t b) {
  unsigned long res;
  static_assert(sizeof(res) == sizeof(uint64_t), "");
  return __builtin_uaddl_overflow(a, b, &res) ? UINT64_MAX : res;
}

uint64_t mul_saturate(uint64_t a, uint64_t b) {
  unsigned long res;
  static_assert(sizeof(res) == sizeof(uint64_t), "");
  return __builtin_umull_overflow(a, b, &res) ? UINT64_MAX : res;
}

uint64_t divide_up(uint64_t n, uint64_t amount) {
  return (n + amount - 1) / amount;
}

uint64_t round_up(uint64_t n, uint64_t amount) {
  return divide_up(n, amount) * amount;
}

}

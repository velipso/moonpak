// SPDX-License-Identifier: 0BSD
//
// Fixed-point helpers.
//
// Fixed-point numbers are represented as ordinary integer types. This struct can be used at
// compile-time or run-time to make conversion and multiplication easier to reason about.
//
// inline constexpr Fix Q{8}; // ideally should be `inline constexpr` but not required
//
// int x = Q(10);        // x = 10 << 8
// int y = Q(20);        // y = 20 << 8
// int z = Q.mul(x, y);  // z = (x * y) >> 8
//
// int a = Q(1.23);      // works if Q is `constexpr` -- double is converted at compile-time
//
#pragma once
#include <stdint.h>

namespace moonpak {

template <typename T, typename WideT>
struct FixT {
  int q;

  constexpr T operator()(int v) const {
    return v << q;
  }

  consteval T operator()(double v) const {
    return (T)(v * (1 << q));
  }

  constexpr T operator()(auto fmt, int v) const {
    return fmt.q < q
      ? v << (q - fmt.q)
      : fmt.q > q
      ? v >> (fmt.q - q)
      : v;
  }

  template <auto fmt>
  constexpr T operator()(int v) const {
    return fmt.q < q
      ? v << (q - fmt.q)
      : fmt.q > q
      ? v >> (fmt.q - q)
      : v;
  }

  constexpr T trunc(int v) const {
    return v < 0 ? -((-v) >> q) : v >> q;
  }

  constexpr T floor(int v) const {
    return v >> q;
  }

  constexpr T frac(int v) const {
    return v & ((1 << q) - 1);
  }

  constexpr T mul(int a, int b) const {
    return (a * b) >> q;
  }

  constexpr T mul(auto fmtA, int a, auto fmtB, int b) const {
    int q2 = fmtA.q + fmtB.q - q;
    return q2 > 0
      ? (a * b) >> q2
      : q2 < 0
      ? (a * b) << -q2
      : a * b;
  }

  template <auto fmtA, auto fmtB>
  constexpr T mul(int a, int b) const {
    int q2 = fmtA.q + fmtB.q - q;
    return q2 > 0
      ? (a * b) >> q2
      : q2 < 0
      ? (a * b) << -q2
      : a * b;
  }

  constexpr T lmul(int a, int b) const {
    return ((WideT)a * (WideT)b) >> q;
  }

  constexpr T lmul(auto fmtA, int a, auto fmtB, int b) const {
    int q2 = fmtA.q + fmtB.q - q;
    return q2 > 0
      ? ((WideT)a * (WideT)b) >> q2
      : q2 < 0
      ? ((WideT)a * (WideT)b) << -q2
      : (WideT)a * (WideT)b;
  }

  template <auto fmtA, auto fmtB>
  constexpr T lmul(int a, int b) const {
    int q2 = fmtA.q + fmtB.q - q;
    return q2 > 0
      ? ((WideT)a * (WideT)b) >> q2
      : q2 < 0
      ? ((WideT)a * (WideT)b) << -q2
      : (WideT)a * (WideT)b;
  }
};

using Fix = FixT<int, int64_t>;
using Fix16 = FixT<int16_t, int>;

} // moonpak

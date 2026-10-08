// SPDX-License-Identifier: 0BSD
#pragma once
#include <stdint.h>

namespace moonpak {

namespace Util {
  extern const int16_t sin14[151];
  extern const uint8_t ctz32_bitpos[32];

  static inline int ctz32(uint32_t v) {
    if (v == 0) return 32;
    return ctz32_bitpos[((v & -v) * 0x077cb531) >> 27];
  }

  static inline int clamp(int v, int min, int max) {
    return v < min ? min : v > max ? max : v;
  }

  static inline int min(int a, int b) {
    return a < b ? a : b;
  }
  static inline int min(int a, int b, int c) {
    return min(min(a, b), c);
  }
  static inline int min(int a, int b, int c, int d) {
    return min(min(a, b), min(c, d));
  }
  static inline int min(int a, int b, int c, int d, int e) {
    return min(min(a, b), min(c, d), e);
  }
  static inline int min(int a, int b, int c, int d, int e, int f) {
    return min(min(a, b), min(c, d), min(e, f));
  }

  static inline int max(int a, int b) {
    return a > b ? a : b;
  }
  static inline int max(int a, int b, int c) {
    return max(max(a, b), c);
  }
  static inline int max(int a, int b, int c, int d) {
    return max(max(a, b), max(c, d));
  }
  static inline int max(int a, int b, int c, int d, int e) {
    return max(max(a, b), max(c, d), e);
  }
  static inline int max(int a, int b, int c, int d, int e, int f) {
    return max(max(a, b), max(c, d), max(e, f));
  }

  static inline int abs(int v) {
    return v < 0 ? -v : v;
  }

  // Divide by constant functions. Searches for multiply+shift operations at compile-time that
  // satisfy dividing -R...R by D. Also handles division by power of 2 using shifts/masking.
  //
  // Divisor (D) must be greater than 0. R defaults to 32768 and can be omitted.
  //
  //   div<D, R>(value)     - regular division, i.e., value / D
  //   mod<D, R>(value)     - regular modulo, i.e., value % D
  //   divmod<D, R>(value)  - returns div + mod results as { q, r }
  //   ediv<D, R>(value)    - euclidean division, i.e., division that always rounds down
  //   emod<D, R>(value)    - euclidean modulo, i.e., modulo that always returns non-negative
  //   edivmod<D, R>(value) - returns ediv + emod results as { q, r }

  struct DivInfo {
    int m;
    int s;
  };

  template <int D, int R = 32768>
  consteval DivInfo findDivInfo() {
    static_assert(D > 0);
    for (int s = 1; s < 31; s++) {
      int m = (1 << s) / D + 1;
      bool good = true;
      for (int v = R; v > 0; v--) {
        int correct = v / D;
        int result = ((uint32_t)v * (uint32_t)m) >> s;
        if (correct != result) {
          good = false;
          break;
        }
      }
      if (good) return { m, s };
    }
    return { 0, 0 };
  }

  template <int D, int R = 32768>
  consteval DivInfo makeDivInfo() {
    constexpr DivInfo d = findDivInfo<D, R>();
    static_assert(d.m != 0, "Unable to find fast division");
    return d;
  }

  template <int D, int R = 32768>
  inline constexpr DivInfo divInfo = makeDivInfo<D, R>();

  struct DivMod {
    int q;
    int r;
  };

  template<int D, int R = 32768>
  static inline int div(int v) {
    static_assert(D > 0);
    if constexpr ((D & (D - 1)) == 0) { // is power of 2?
      constexpr int s = __builtin_ctz(D);
      return v < 0 ? -((-v) >> s) : (v >> s);
    } else {
      constexpr auto d = divInfo<D, R>;
      return v < 0
        ? -(int)(((uint32_t)-v * (uint32_t)d.m) >> d.s)
        :  (int)(((uint32_t) v * (uint32_t)d.m) >> d.s);
    }
  }

  template <int D, int R = 32768>
  static inline int mod(int v) {
    static_assert(D > 0);
    return v - div<D, R>(v) * D;
  }

  template <int D, int R = 32768>
  static inline DivMod divmod(int v) {
    static_assert(D > 0);
    int q = div<D, R>(v);
    int r = v - q * D;
    return { q, r };
  }

  template <int D, int R = 32768>
  static inline int ediv(int v) {
    static_assert(D > 0);
    if constexpr ((D & (D - 1)) == 0) { // is power of 2?
      return v >> __builtin_ctz(D);
    } else {
      int q = div<D, R>(v);
      int r = v - q * D;
      return q - (r < 0 ? 1 : 0);
    }
  }

  template <int D, int R = 32768>
  static inline int emod(int v) {
    static_assert(D > 0);
    if constexpr ((D & (D - 1)) == 0) { // is power of 2?
      return v & (D - 1);
    } else {
      int r = mod<D, R>(v);
      return r < 0 ? r + D : r;
    }
  }

  template <int D, int R = 32768>
  static inline DivMod edivmod(int v) {
    static_assert(D > 0);
    if constexpr ((D & (D - 1)) == 0) { // is power of 2?
      return { v >> __builtin_ctz(D), v & (D - 1) };
    } else {
      int q = div<D, R>(v);
      int r = v - q * D;
      if (r < 0) {
        q--;
        r += D;
      }
      return { q, r };
    }
  }
}

} // moonpak

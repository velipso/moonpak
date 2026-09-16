// SPDX-License-Identifier: 0BSD
#include "Rnd.hpp"

namespace Rnd {
  uint32_t seed = 1;
  uint32_t i = 1;

  uint32_t hash(uint32_t i0) {
    uint32_t z0 = (i0 * 1831267127) ^ i0;
    uint32_t z1 = (z0 * 3915839201) ^ (z0 >> 20);
    uint32_t z2 = (z1 * 1561867961) ^ (z1 >> 24);
    return z2;
  }

  uint32_t hash(uint32_t i0, uint32_t i1) {
    uint32_t z0 = (i1 * 1833778363) ^ i0;
    uint32_t z1 = (z0 *  337170863) ^ (z0 >> 13) ^ z0;
    uint32_t z2 = (z1 *  620363059) ^ (z1 >> 10);
    uint32_t z3 = (z2 *  232140641) ^ (z2 >> 21);
    return z3;
  }

  uint32_t hash(uint32_t i0, uint32_t i1, uint32_t i2) {
    uint32_t z0 = (i0 *  460493531) ^ i1;
    uint32_t z1 = (z0 * 4217964173) ^ (i2 >>  4);
    uint32_t z2 = (i2 *  151288633) ^ (z1 >> 11) ^ z1;
    uint32_t z3 = (z2 * 1894377257) ^ (z2 >> 11);
    uint32_t z4 = (z3 * 1640414617) ^ (z3 >> 18);
    return z4;
  }

  uint32_t roll(uint32_t sides) {
    if (sides <= 1) return 0;
    uint32_t mask = sides - 1;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    for (;;) {
      uint32_t v = next() & mask;
      if (v < sides) return v;
    }
  }
}

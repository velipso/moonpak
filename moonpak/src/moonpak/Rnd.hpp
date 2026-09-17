// SPDX-License-Identifier: 0BSD
//
// Simple and fast random number generation.
//
// Inside vblank:
//
// Rnd::update();
//
// Then anywhere else:
//
// Rnd::next();  // next random number (32 random bits)
// Rnd::roll(6); // roll a 6-sided dice (returns 0-5)
//
// There are also hash functions which can be used for procedural generation:
//
// Rnd::hash(x);        // hash a single number
// Rnd::hash(x, y);     // hash two numbers
// Rnd::hash(x, y, z);  // hash three numbers
//
// For example, to generate a random 2D array:
//
// uint32_t seed = Rnd::next();
// for (int y = 0; y < 1000; y++) {
//   for (int x = 0; x < 1000; x++) {
//     uint32_t value = Rnd::hash(x, y, seed);
//   }
// }
//
// The benefit of using `hash` instead of `next` is that the array can be queried in random order
// without storing it in memory, i.e.,
//
// // get the value at (234, 546) without having to pre-generate thousands of numbers in order:
// Rnd::hash(234, 546, seed);
//
#pragma once
#include "moonpak/Inp.hpp"
#include <stdint.h>

namespace Rnd {
  extern uint32_t seed;
  extern uint32_t i;

  uint32_t hash(uint32_t i0);
  uint32_t hash(uint32_t i0, uint32_t i1);
  uint32_t hash(uint32_t i0, uint32_t i1, uint32_t i2);
  uint32_t roll(uint32_t sides);

  inline void save(uint32_t *seedOut, uint32_t *iOut) {
    *seedOut = seed;
    *iOut = i;
  }

  inline void restore(uint32_t seedValue, uint32_t iValue) {
    seed = seedValue;
    i = iValue;
  }

  inline void update() {
    seed = hash(Inp::value, seed, i++);
  }

  inline uint32_t next() {
    return hash(seed, i++);
  }
}

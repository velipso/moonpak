// SPDX-License-Identifier: 0BSD
#pragma once
#include <stdint.h>

#ifdef TESTS

// define fake atomic actions for test suite
// (also good for documenting the logic behind the actual atomic functions)

static inline bool atomicBitSet(uint32_t *addr, uint32_t mask) {
  if ((*addr & mask) == 0) {
    *addr |= mask;
    return true;
  }
  return false;
}

static inline bool atomicBitClear(uint32_t *addr, uint32_t mask) {
  if ((*addr & mask) == mask) {
    *addr &= ~mask;
    return true;
  }
  return false;
}

#else

extern "C" {
  // these "atomic" actions are implemented by disabling interrupts (via CPSR) in the critical
  // section of code
  extern bool moonpak_atomicBitSet(uint32_t *addr, uint32_t mask);
  extern bool moonpak_atomicBitClear(uint32_t *addr, uint32_t mask);
}
#define atomicBitSet    moonpak_atomicBitSet
#define atomicBitClear  moonpak_atomicBitClear
#endif // TESTS

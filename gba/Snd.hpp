// SPDX-License-Identifier: 0BSD
#pragma once
#include <stdint.h>

struct Snd {
#ifdef PLATFORM_GBA
  int8_t bufferDMA[1216];
#endif
  int16_t bufferTemp[552];
  int frameCount;
  int dmaWriteIndex;
  int masterVolume; // 0-16
  int sfxVolume; // 0-16
  int songVolume; // 0-16

  Snd &reset();
  Snd() { reset(); }
  Snd &tick();

#ifdef TESTS
  static int test(bool verbose);
#endif
};

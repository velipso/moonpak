// SPDX-License-Identifier: 0BSD
#pragma once
#include "moonpak/Snd.hpp"
#include <stdint.h>

void sndRenderAdpcmSet(
  int16_t *out,
  uint32_t samples, // any length
  int volume,       // 0-256
  uint32_t *state,
  const uint8_t **data
);
void sndRenderAdpcmAdd(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *state,
  const uint8_t **data
);

void sndRenderWaveTableSet1024(
  int16_t *out,
  uint32_t samples, // must be divisible by 4
  int volume,       // 0-256
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd1024(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet512(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd512(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet256(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd256(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet128(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd128(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

extern "C" void sndRenderNoiseSet(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  uint32_t *state
);
extern "C" void sndRenderNoiseAdd(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  uint32_t *state
);

static inline int sndAdpcmStateFromHeader(const uint8_t *data) {
  int index = data[2];
  if (index > 88) index = 88;
  return data[0] | (data[1] << 8) | (index << 16);
}

static inline int16_t sndAdpcmFirstSample(int state) {
  return (int16_t)(state & 0xffff);
}

static inline uint32_t sndSampleCountPerFrame(int frame) {
  // sample rate is 32768 samples/sec, or 1 sample every 512 cycles, so target samples per frame:
  // 280896 cycles per frame / 512 cycles per sample = 548.625 samples per frame
  // this is spread over 32 frames, most frames having 548 samples, but some having 552 samples:
  frame &= 0x1f;
  return frame == 6 || frame == 12 || frame == 19 || frame == 25 || frame == 31
    ? 552
    : 548;
}

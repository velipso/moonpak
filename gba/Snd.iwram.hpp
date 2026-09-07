// SPDX-License-Identifier: 0BSD
#pragma once
#include <stdint.h>

void sndRenderAdpcmSet(
  int16_t *out,
  uint32_t samples, // any length
  uint32_t volume,  // 0-256
  uint32_t *state,
  uint8_t *&data
);
void sndRenderAdpcmAdd(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *state,
  uint8_t *&data
);

void sndRenderWaveTableSet1024(
  int16_t *out,
  uint32_t samples, // must be divisible by 4
  uint32_t volume,  // 0-256
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd1024(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet512(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd512(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet256(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd256(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

void sndRenderWaveTableSet128(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);
void sndRenderWaveTableAdd128(
  int16_t *out,
  uint32_t samples,
  uint32_t volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
);

static inline int sndAdpcmStateFromHeader(uint8_t *data) {
  int index = data[2];
  if (index > 88) index = 88;
  return data[0] | (data[1] << 8) | (index << 16);
}

static inline int16_t sndAdpcmFirstSample(int state) {
  return (int16_t)(state & 0xffff);
}

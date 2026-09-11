// SPDX-License-Identifier: 0BSD
#include "Snd.iwram.hpp"

static const int16_t adpcmStepSize[] = {
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
  80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
  544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
  2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
  12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t adpcmIndex[] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

void sndRenderAdpcmSet(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *state,
  const uint8_t **data
) {
  int sample = (int16_t)(*state & 0xffff);
  int index = (*state >> 16) & 0x7f;
  bool secondHalf = *state & 0x00800000;
  const uint8_t *dataPtr = *data;
  while (samples > 0) {
    int nibble;
    if (secondHalf) {
      nibble = *dataPtr >> 4;
      dataPtr++;
    } else {
      nibble = *dataPtr & 15;
    }
    secondHalf = !secondHalf;

    int stepSize = adpcmStepSize[index];
    index += adpcmIndex[nibble];
    if (index < 0) index = 0;
    else if (index > 88) index = 88;

    int difference = 0;
    if (nibble & 4) difference += stepSize;
    if (nibble & 2) difference += stepSize >> 1;
    if (nibble & 1) difference += stepSize >> 2;
    difference += stepSize >> 3;
    if (nibble & 8) difference = -difference;
    sample += difference;
    if (sample > 32767) sample = 32767;
    else if (sample < -32768) sample = -32768;

    *out++ = (sample * volume) >> 8; // SET
    samples--;
  }
  *data = dataPtr;
  *state = (secondHalf ? 0x00800000 : 0) | (index << 16) | (sample & 0xffff);
}

void sndRenderAdpcmAdd(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *state,
  const uint8_t **data
) {
  int sample = (int16_t)(*state & 0xffff);
  int index = (*state >> 16) & 0x7f;
  bool secondHalf = *state & 0x00800000;
  const uint8_t *dataPtr = *data;
  while (samples > 0) {
    int nibble;
    if (secondHalf) {
      nibble = *dataPtr >> 4;
      dataPtr++;
    } else {
      nibble = *dataPtr & 15;
    }
    secondHalf = !secondHalf;

    int stepSize = adpcmStepSize[index];
    index += adpcmIndex[nibble];
    if (index < 0) index = 0;
    else if (index > 88) index = 88;

    int difference = 0;
    if (nibble & 4) difference += stepSize;
    if (nibble & 2) difference += stepSize >> 1;
    if (nibble & 1) difference += stepSize >> 2;
    difference += stepSize >> 3;
    if (nibble & 8) difference = -difference;
    sample += difference;
    if (sample > 32767) sample = 32767;
    else if (sample < -32768) sample = -32768;

    *out++ += (sample * volume) >> 8; // ADD
    samples--;
  }
  *data = dataPtr;
  *state = (secondHalf ? 0x00800000 : 0) | (index << 16) | (sample & 0xffff);
}

void sndRenderWaveTableSet1024(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] = (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[1] = (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[2] = (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[3] = (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableAdd1024(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] += (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[1] += (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[2] += (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    out[3] += (volume * waveTable[p >> 22]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableSet512(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] = (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[1] = (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[2] = (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[3] = (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableAdd512(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] += (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[1] += (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[2] += (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    out[3] += (volume * waveTable[p >> 23]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableSet256(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] = (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[1] = (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[2] = (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[3] = (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableAdd256(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] += (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[1] += (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[2] += (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    out[3] += (volume * waveTable[p >> 24]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableSet128(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] = (volume * waveTable[p >> 25]) >> 8; // SET
    p += dphase;
    out[1] = (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    out[2] = (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    out[3] = (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

void sndRenderWaveTableAdd128(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  const int16_t *waveTable
) {
  uint32_t p = *phase;
  while (samples > 0) {
    out[0] += (volume * waveTable[p >> 25]) >> 8; // ADD
    p += dphase;
    out[1] += (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    out[2] += (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    out[3] += (volume * waveTable[p >> 25]) >> 8;
    p += dphase;
    samples -= 4;
    out += 4;
  }
  *phase = p;
}

#ifdef PLATFORM_HOST
extern "C" void sndRenderNoiseSet(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  uint32_t *state
) {
  uint32_t p = *phase;
  uint32_t s = *state;
  // this PRNG may look complex, but the assembly version is only two instructions, so very fast
  // to implement on the GBA
  #define STEP()  do {                            \
      const uint32_t old = p;                     \
      p += dphase;                                \
      if (p < old) {                              \
        s = (s >> 1) ^ (-(s & 1) & 0x80200003u);  \
      }                                           \
    } while (0)
  while (samples > 0) {
    out[0] = (volume * (((int32_t)s) >> 16)) >> 8; // SET
    STEP();
    out[1] = (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    out[2] = (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    out[3] = (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    samples -= 4;
    out += 4;
  }
  #undef STEP
  *phase = p;
  *state = s;
}

extern "C" void sndRenderNoiseAdd(
  int16_t *out,
  uint32_t samples,
  int volume,
  uint32_t *phase,
  uint32_t dphase,
  uint32_t *state
) {
  uint32_t p = *phase;
  uint32_t s = *state;
  #define STEP()  do {                            \
      const uint32_t old = p;                     \
      p += dphase;                                \
      if (p < old) {                              \
        s = (s >> 1) ^ (-(s & 1) & 0x80200003u);  \
      }                                           \
    } while (0)
  while (samples > 0) {
    out[0] += (volume * (((int32_t)s) >> 16)) >> 8; // ADD
    STEP();
    out[1] += (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    out[2] += (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    out[3] += (volume * (((int32_t)s) >> 16)) >> 8;
    STEP();
    samples -= 4;
    out += 4;
  }
  #undef STEP
  *phase = p;
  *state = s;
}
#endif

int Snd::tick() {
  int samples = sndSampleCountPerFrame(frameCount++);
  bool first = true;
  if (masterVolume > 0) {
    for (int i = 0; i < maxSongs; i++) {
      SndSong &sndSong = songs[i];
      if (sndSong.isEnabled()) {
        first = sndSong.tick(bufferTemp, samples, masterVolume * songVolume, first);
      }
    }
    for (int i = 0; i < maxSfxs; i++) {
      SndPCM &sfx = sfxs[i];
      if (sfx.isEnabled()) {
        first = sfx.render(bufferTemp, samples, masterVolume * sfxVolume, first);
      }
    }
  }
  if (first) {
    // nothing was rendered into the buffer, so we need to clear it
    for (int i = 0; i < samples; i++) {
      bufferTemp[i] = 0;
    }
  }
  return samples;
}

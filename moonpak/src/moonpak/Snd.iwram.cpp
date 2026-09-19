// SPDX-License-Identifier: 0BSD
#include "moonpak/Snd.iwram.hpp"
#ifdef PLATFORM_GBA
#include "moonpak/Reg.hpp"
// on GBA, just set/add the sample directly (fast)
#define SAMPLE_SET(out, value)  *out++ = (value) >> MOONPAK_SAMPLE_SHIFT
#define SAMPLE_ADD(out, value)  *out++ += (value) >> MOONPAK_SAMPLE_SHIFT
#else
// on host, clamp every sample
#define SAMPLE_SET(out, value)  do {                   \
    int v = (value) >> MOONPAK_SAMPLE_SHIFT;           \
    if (v < -32768) v = -32768;                        \
    else if (v > 32767) v = 32767;                     \
    *out++ = v;                                        \
  } while (0)
#define SAMPLE_ADD(out, value)  do {                   \
    int v = *out + ((value) >> MOONPAK_SAMPLE_SHIFT);  \
    if (v < -32768) v = -32768;                        \
    else if (v > 32767) v = 32767;                     \
    *out++ = v;                                        \
  } while (0)
#endif

struct AdpcmTable {
  int value[89][16];
};

consteval AdpcmTable makeAdpcmTable() {
  AdpcmTable table{};

  constexpr int8_t indexDelta[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

  constexpr int16_t stepSize[] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
    80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
    544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
  };

  for (int index = 0; index < 89; index++) {
    int step = stepSize[index];
    for (int nibble = 0; nibble < 16; nibble++) {
      int difference = step >> 3;
      if (nibble & 4) difference += step;
      if (nibble & 2) difference += step >> 1;
      if (nibble & 1) difference += step >> 2;
      if (nibble & 8) difference = -difference;
      int nextIndex = index + indexDelta[nibble];
      if (nextIndex < 0) nextIndex = 0;
      else if (nextIndex > 88) nextIndex = 88;
      // bits 0..7 = next index
      // bits 15..31 = signed 17-bit difference
      table.value[index][nibble] = (difference * 32768) + nextIndex;
    }
  }

  return table;
}

static constexpr auto adpcmTable = makeAdpcmTable();

#define ADPCM_DECODE(SETTER)  do {                \
    int entry = adpcmTable.value[index][nibble];  \
    int difference = entry >> 15;                 \
    index = entry & 0x7f;                         \
    sample += difference;                         \
    if (sample > 32767) sample = 32767;           \
    else if (sample < -32768) sample = -32768;    \
    SETTER(out, sample * volume);                 \
    samples--;                                    \
  } while (0)

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

  // finish a byte left half-consumed by the previous call
  if (secondHalf && samples > 0) {
    int nibble = *dataPtr >> 4;
    dataPtr++;
    ADPCM_DECODE(SAMPLE_SET);
    secondHalf = false;
  }

  while (samples >= 2) {
    int value = *dataPtr++;
    int nibble = value & 15;
    ADPCM_DECODE(SAMPLE_SET);
    nibble = value >> 4;
    ADPCM_DECODE(SAMPLE_SET);
  }

  // leave the high nibble pending if there's one sample left
  if (samples > 0) {
    int nibble = *dataPtr & 15;
    ADPCM_DECODE(SAMPLE_SET);
    secondHalf = true;
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

  // finish a byte left half-consumed by the previous call
  if (secondHalf && samples > 0) {
    int nibble = *dataPtr >> 4;
    dataPtr++;
    ADPCM_DECODE(SAMPLE_ADD);
    secondHalf = false;
  }

  while (samples >= 2) {
    int value = *dataPtr++;
    int nibble = value & 15;
    ADPCM_DECODE(SAMPLE_ADD);
    nibble = value >> 4;
    ADPCM_DECODE(SAMPLE_ADD);
  }

  // leave the high nibble pending if there's one sample left
  if (samples > 0) {
    int nibble = *dataPtr & 15;
    ADPCM_DECODE(SAMPLE_ADD);
    secondHalf = true;
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
    SAMPLE_SET(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 22]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_ADD(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 22]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 22]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_SET(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 23]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_ADD(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 23]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 23]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_SET(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 24]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_ADD(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 24]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 24]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_SET(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_SET(out, volume * waveTable[p >> 25]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_ADD(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 25]);
    p += dphase;
    SAMPLE_ADD(out, volume * waveTable[p >> 25]);
    p += dphase;
    samples -= 4;
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
    SAMPLE_SET(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_SET(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_SET(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_SET(out, volume * (((int32_t)s) >> 16));
    STEP();
    samples -= 4;
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
    SAMPLE_ADD(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_ADD(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_ADD(out, volume * (((int32_t)s) >> 16));
    STEP();
    SAMPLE_ADD(out, volume * (((int32_t)s) >> 16));
    STEP();
    samples -= 4;
  }
  #undef STEP
  *phase = p;
  *state = s;
}
#endif

uint32_t Snd::tick() {
  uint32_t samples = sndSampleCountPerFrame(frameCount++);
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
    for (uint32_t i = 0; i < samples; i++) {
      bufferTemp[i] = 0;
    }
  }
  return samples;
}

#ifdef PLATFORM_GBA
#if 1
// use assembly version
extern "C" void sndQuantize8(int8_t *bufferDMA, uint32_t samples, int16_t *bufferTemp);
#else
static inline void sndQuantize8CPP(int8_t *bufferDMA, uint32_t samples, int16_t *bufferTemp) {
  while (samples-- > 0) {
    int sample = (*bufferTemp++) >> (16 - SAMPLE_SHIFT);
    *bufferDMA++ = sample < -128 ? -128 : sample > 127 ? 127 : sample;
  }
}
#endif

void Snd::copy() {
  if (bufferState == 0xff) {
    // if we haven't initialized yet, then spend this frame initializing and letting the DMA get
    // ahead a frame
    init();
    return;
  }
  uint32_t samples = tick();
  int16_t *read = bufferTemp;
  while (samples > 0) {
    uint32_t write = sizeof(bufferDMA) - bufferIndex;
    if (write > samples) write = samples;
    samples -= write;
    sndQuantize8(&bufferDMA[bufferIndex], write, read);
    read += write;
    bufferIndex += write;
    if (bufferIndex >= sizeof(bufferDMA)) {
      bufferIndex -= sizeof(bufferDMA);
    }
  }
}

void Snd::timer1Handler() {
  Snd &snd = *Snd::global;

  snd.bufferState++;
  if (snd.bufferState >= Snd::bufferDMACount) {
    snd.bufferState = 0;
  }
  uint32_t source = (uint32_t)&snd.bufferDMA[snd.bufferState * 608];

  Reg::DMA1CNT_H::set(0);
  Reg::DMA1SAD::set(source);
  Reg::DMA1CNT_H::write()
    .destControl(2)   // fixed destination
    .sourceControl(0) // incremenet source
    .repeat(1)
    .word32(1)
    .timing(3) // sound FIFO
    .enable(1)
    .done();
}
#endif

// SPDX-License-Identifier: 0BSD
#include "Snd.hpp"
#include "Snd.iwram.hpp"
#include "SndData.hpp"
#include <stdlib.h>

#ifdef TESTS
#include <random>
#include <stdio.h>
#include "data/songs/outro.hpp" // TODO: remove
#include "data/songs/basic.hpp" // TODO: remove
static bool g_verbose;
#define log(fmt, ...) if (g_verbose) printf(fmt, ##__VA_ARGS__)
#else
#define log(fmt, ...)
#endif

struct FamiHeader {
  uint32_t magic;
  uint16_t instrumentsLength;
  uint16_t songsLength;
  uint32_t instrumentsOffset;
  uint32_t songsOffset;
};

struct FamiInstrument {
  uint8_t envelopesLength;
  uint8_t reserved[3];
  uint32_t envelopesOffset[];
};

struct FamiEnvelope {
  uint8_t kind;
  uint8_t reserved;
  int16_t loop;
  int16_t release;
  uint16_t valuesLength;
  union {
    int8_t ivalues[];
    uint8_t uvalues[];
  };
};

struct FamiSong {
  uint16_t loopChannelsLength; // loop:12, channelsLength:4
  uint16_t songLength;
  uint32_t channelsOffset[];
};

struct FamiChannel {
  uint8_t kind;
  uint8_t initialVolume;
  uint16_t patternsLength;
  int16_t instances[];
};

struct SndChannel {
  const uint16_t *events;
  const FamiInstrument *instrument;
  struct {
    int16_t cursor;
    int8_t index;
    union {
      int8_t ivalue;
      uint8_t uvalue;
    };
  } env[7];
  uint8_t kind;
  uint8_t state; // stop(0), on(1), release(2)
  int16_t wait;
  int16_t pitch;
  int16_t release;
  int16_t duration;
  int16_t volume;
  uint32_t phase;

  static constexpr int EnvVolume    = 0;
  static constexpr int EnvArpeggio  = 1;
  static constexpr int EnvPitchAbs  = 2;
  static constexpr int EnvPitchRel  = 3;
  static constexpr int EnvDutyCycle = 4;
  static constexpr int EnvWave      = 5;
  static constexpr int EnvRepeat    = 6;

  void reset(int ki, int vo, const uint16_t *ev) {
    kind = ki;
    volume = vo;
    events = ev;
    instrument = nullptr;
    wait = 0;
    pitch = -1;
    state = 0;
    resetEnvelopes();
  }

  void disable() {
    events = nullptr;
  }

  bool isEnabled() {
    return events != nullptr;
  }

  void resetEnvelopes() {
    for (int i = 0; i < 7; i++) {
      env[i].cursor = 0;
      env[i].ivalue = 0;
      env[i].index = -1;
    }
    env[EnvVolume].uvalue = 255;
    if (!instrument) return;
    for (int en = 0; en < instrument->envelopesLength; en++) {
      const FamiEnvelope *fenv = famiEnvelope(en);
      env[fenv->kind].index = en;
      env[fenv->kind].ivalue = fenv->ivalues[0];
    }
  }

  const FamiEnvelope *famiEnvelope(int en) {
    return instrument
      ? (const FamiEnvelope *)&(((const uint8_t *)instrument)[instrument->envelopesOffset[en]])
      : nullptr;
  }

  void famiInstrument(const FamiInstrument *finst) {
    instrument = finst;
    resetEnvelopes();
  }

  void note(int value, int rel, int dur, bool attack) {
    pitch = value << 4;
    release = rel;
    duration = dur;
    state = 1; // note on
    if (attack) {
      phase = 0;
      resetEnvelopes();
    }
  }

  bool render(int16_t *out, int samples, bool first) {
    if (!state) return first;

    if (kind < 4) {
      // oscillator
      int p = pitch + (env[EnvPitchAbs].ivalue << 1);
      int duty = env[EnvDutyCycle].uvalue;
      int wkind = SndData::waveKind(kind, duty);
      int band = SndData::waveBand(p);
      int renderSize;
      const int16_t *waveTable = SndData::waveTable(wkind, band, &renderSize);
      uint32_t dphase = SndData::frequencyPerPitch[p];
      dphase = (dphase << 14) | (dphase >> 2);
      int vol = (volume * env[EnvVolume].uvalue) >> 8;
      if (renderSize == 128) {
        if (first) {
          sndRenderWaveTableSet128(out, samples, vol, &phase, dphase, waveTable);
        } else {
          sndRenderWaveTableAdd128(out, samples, vol, &phase, dphase, waveTable);
        }
      } else if (renderSize == 256) {
        if (first) {
          sndRenderWaveTableSet256(out, samples, vol, &phase, dphase, waveTable);
        } else {
          sndRenderWaveTableAdd256(out, samples, vol, &phase, dphase, waveTable);
        }
      } else if (renderSize == 512) {
        if (first) {
          sndRenderWaveTableSet512(out, samples, vol, &phase, dphase, waveTable);
        } else {
          sndRenderWaveTableAdd512(out, samples, vol, &phase, dphase, waveTable);
        }
      } else { // 1024
        if (first) {
          sndRenderWaveTableSet1024(out, samples, vol, &phase, dphase, waveTable);
        } else {
          sndRenderWaveTableAdd1024(out, samples, vol, &phase, dphase, waveTable);
        }
      }
      first = false;
    } else if (kind == 4) {
      // TODO: Wave
    } else if (kind == 5) {
      // TODO: PCM
    } else if (kind == 6) {
      // TODO: Noise
    }

    duration--;
    if (duration <= 0) {
      state = 0; // note stop
      return first;
    } else if (release > 0) {
      release--;
      if (release <= 0) {
        state = 2; // note release
      }
    }

    // advance envelopes
    for (int i = 0; i < 7; i++) {
      if (env[i].index < 0) continue;
      env[i].cursor++;
      const FamiEnvelope *fenv = famiEnvelope(env[i].index);
      if ((state == 1 && env[i].cursor >= fenv->release) || env[i].cursor >= fenv->valuesLength) {
        env[i].cursor = state == 1 ? fenv->loop : fenv->valuesLength - 1;
      }
      env[i].ivalue = fenv->ivalues[env[i].cursor];
      if (i == EnvPitchRel) {
        pitch += env[i].ivalue;
      }
    }

    return first;
  }
};

struct SndSong {
  const FamiHeader &fami;
  const FamiSong &song;
  int column;
  SndChannel channels[16];

  SndSong(const FamiHeader &fami, const FamiSong &song) : fami(fami), song(song) {
    loadColumn(0);
  }

  const uint8_t *root() {
    return (const uint8_t *)&fami;
  }

  void loadColumn(int col) {
    column = col;

    int channelsLength = song.loopChannelsLength & 15;
    for (int ch = 0; ch < 16; ch++) {
      if (ch > channelsLength) { // channelsLength is off by one (intentional)
        channels[ch].disable();
        continue;
      }

      const FamiChannel &channel = *(const FamiChannel *)&root()[song.channelsOffset[ch]];
      int pa = channel.instances[column];
      if (pa < 0) {
        channels[ch].disable();
        continue;
      }

      const uint32_t *patternsOffset = (const uint32_t *)&root()[
        song.channelsOffset[ch] +
        sizeof(FamiChannel) +
        sizeof(uint16_t) * song.songLength
      ];

      channels[ch].reset(
        channel.kind,
        channel.initialVolume,
        (const uint16_t *)&root()[patternsOffset[pa]]
      );
    }
  }

  void tick(int16_t *out, int samples) {
    bool patternEnd = false;
    for (int ch = 0; ch < 16; ch++) {
      SndChannel &channel = channels[ch];
      if (!channel.isEnabled()) continue;
      if (channel.wait > 0) {
        channel.wait--;
        if (channel.wait > 0) continue;
      }
      for (;;) {
        uint16_t ev = *channel.events++;
        if ((ev & 0x8000) == 0) {
          // double-payload
          uint16_t e2 = *channel.events++;
          int note = ev >> 8;
          int release = (ev & 0xff) | ((e2 >> 3) & 0x700);
          int duration = e2 & 0x7ff;
          bool attack = (e2 & 0x8000) != 0;
          bool autowait = (e2 & 0x4000) != 0;
          channel.note(note, release, duration, attack);
          //log("[%d] NOTE %d/%d/%d %s%s\n",
          //  ch, note, release, duration, attack ? "A" : "x", autowait ? "W" : "x");
          if (autowait && duration > 0) {
            channel.wait += duration;
            goto next_channel;
          }
        } else {
          // single-payload
          int param = ev & 0xff;
          switch ((ev >> 8) & 0x7f) {
            case 0x00: // WAIT
              channel.wait += param + 1;
              goto next_channel;
            case 0x01: // PATEND
              patternEnd = true;
              goto next_channel;
            case 0x02: { // INST1
set_instrument:
              const uint32_t *instOffset = (const uint32_t *)&root()[fami.instrumentsOffset];
              channel.famiInstrument((const FamiInstrument *)&root()[instOffset[param]]);
              break;
            }
            case 0x03: // INST2
              param += 256;
              goto set_instrument;
            case 0x04: // VOL
              channel.volume = param;
              break;
          }
        }
      }
next_channel:;
    }

    bool first = true;
    for (int ch = 0; ch < 16; ch++) {
      SndChannel &channel = channels[ch];
      if (!channel.isEnabled()) continue;
      first = channel.render(out, samples, first);
    }
    if (first) {
      for (int i = 0; i < samples; i++) {
        out[i] = 0;
      }
    }

    if (patternEnd) {
      column++;
      if (column >= song.songLength) {
        column = song.loopChannelsLength >> 4;
      }
      loadColumn(column);
    }
  }
};

Snd &Snd::reset() {
  return *this;
}

Snd &Snd::tick() {
  return *this;
}

#ifdef TESTS
static std::mt19937 rng(std::random_device{}());
static int rand(int size) {
  if (size <= 1) return 0;
  return std::uniform_int_distribution<int>(0, size - 1)(rng);
}

static void writeWAV(const char *file, int16_t *data, int size) {
  FILE *fp = fopen(file, "wb");
  #define U32(val)  do {            \
      uint32_t v = val;             \
      fputc(v & 0xff, fp);          \
      fputc((v >> 8) & 0xff, fp);   \
      fputc((v >> 16) & 0xff, fp);  \
      fputc((v >> 24) & 0xff, fp);  \
    } while (0)
  #define U16(val)  do {            \
      uint32_t v = val;             \
      fputc(v & 0xff, fp);          \
      fputc((v >> 8) & 0xff, fp);   \
    } while (0)
  U32(0x46464952);    // 'RIFF'
  U32(size * 2 + 36); // file size minus 'RIFF'
  U32(0x45564157);    // 'WAVE'
  U32(0x20746d66);    // 'fmt '
  U32(16);            // size of fmt chunk
  U16(1);             // audio format
  U16(1);             // mono
  U32(32768);         // sample rate
  U32(32768 * 2);     // bytes per second
  U16(2);             // block align
  U16(16);            // bits per sample
  U32(0x61746164);    // 'data'
  U32(size * 2);      // size of data chunk
  for (int i = 0; i < size; i++) {
    U16(data[i]);
  }
  #undef U32
  #undef U16
  fclose(fp);
}

static int16_t *g_out = NULL;
static int g_outSize = 0;
static int g_outTotal = 0;
static void push(int16_t v) {
  if (g_outSize >= g_outTotal) {
    g_outTotal += 2000;
    g_out = (int16_t *)realloc(g_out, sizeof(int16_t) * g_outTotal);
  }
  g_out[g_outSize++] = v;
}

int Snd::test(bool verbose) {
  g_verbose = verbose;

  int songIndex = 0;

  const uint8_t *fami = dataSongsBasic;
  const FamiHeader &header = *(const FamiHeader *)fami;

  int songsOffset = header.songsOffset;
  songsOffset += songIndex * 4;
  int songOffset = *(uint32_t *)&fami[songsOffset];
  const FamiSong &song = *(const FamiSong *)&fami[songOffset];

  SndSong sndSong(header, song);
  for (int t = 0; t < 5000; t++) {
    int16_t sndOut[552];
    sndSong.tick(sndOut, 552);
    for (int i = 0; i < 552; i++) {
      push(sndOut[i]);
    }
  }

  writeWAV("temp/sndout.wav", g_out, g_outSize);

/*
  example WAV parsing with IMA ADPCM compression

  ffmpeg -i input.wav \
    -ac 1 \
    -ar 32768 \
    -c:a adpcm_ima_wav \
    -map_metadata -1 \
    -fflags +bitexact \
    output.wav

  FILE *fp = fopen("temp/accept.ima.wav", "rb");
  if (!fp) {
    log("failed to open\n");
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  uint8_t *data = (uint8_t *)calloc(size, 1);
  uint8_t *dataEnd = &data[size];
  fread(data, 1, size, fp);
  fclose(fp);

  data += 4; // skip RIFF
  uint32_t fileSize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  data += 8; // fileSize + WAVE
  uint16_t align;
  uint32_t sampleCount;
  while (data < dataEnd) {
    uint32_t chunkName = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    data += 4;
    uint32_t chunkSize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    data += 4;
    printf("%04x %c%c%c%c %d\n", chunkName, chunkName & 0xff, (chunkName >> 8) & 0xff,
      (chunkName >> 16) & 0xff, (chunkName >> 24) & 0xff, chunkSize);
    if (chunkName == 0x20746d66) { // "fmt "
      uint16_t comp = data[0] | (data[1] << 8);
      uint16_t channels = data[2] | (data[3] << 8);
      uint32_t rate = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
      uint32_t bps = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
      align = data[12] | (data[13] << 8);
      uint16_t bitps = data[14] | (data[15] << 8);
      uint16_t ext = data[16] | (data[17] << 8);
      printf("comp %02X channels %d rate %d\n", comp, channels, rate);
      printf("bps %d align %d bitps %d ext %d\n", bps, align, bitps, ext);
    } else if (chunkName == 0x74636166) { // "fact"
      sampleCount = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      printf("sampleCount? %d\n", sampleCount);
    } else if (chunkName == 0x61746164) { // "data"
      uint8_t *blockData = data;
      int16_t *output = (int16_t *)calloc(sampleCount, sizeof(int16_t));
      int16_t *outputPtr = output;
      int sampleLeft = sampleCount;
      while (sampleLeft > 0) {
        int state = sndAdpcmStateFromHeader(blockData);
        blockData += 4;
        *outputPtr++ = sndAdpcmFirstSample(state);
        sampleLeft--;
        if (sampleLeft <= 0) break;
        int outputCount = (align - 4) << 1;
        if (outputCount > sampleLeft) outputCount = sampleLeft;
        sndRenderAdpcmSet(outputPtr, outputCount, 256, state, blockData);
        // sndRenderAdpcmSet will advance blockData
        sampleLeft -= outputCount;
        outputPtr += outputCount;
      }
      fp = fopen("temp/accept.ima.raw", "wb");
      if (fp) {
        fwrite(output, sizeof(int16_t), sampleCount, fp);
        fclose(fp);
      }
    }
    data += chunkSize;
  }
*/

  return 0;
}
#endif

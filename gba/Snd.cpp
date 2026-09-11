// SPDX-License-Identifier: 0BSD
#include "Snd.hpp"
#include "Snd.iwram.hpp"
#include "SndData.hpp"
#include "data/dpcm/DpcmTable.hpp"
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
    uint16_t cursorIndex; // cursor:13, index:3
    union {
      int16_t ivalue;
      uint16_t uvalue;
    };
  } env[7];
  uint8_t kind;
  uint8_t state; // stop(0), on(1), release(2)
  uint8_t noteValue;
  uint8_t reserved;
  int16_t wait;
  int16_t release;
  int16_t duration;
  int16_t volume;
  uint32_t phase;
  uint32_t noiseState;

  static constexpr int EnvVolume    = 0;
  static constexpr int EnvArpeggio  = 1;
  static constexpr int EnvPitchAbs  = 2;
  static constexpr int EnvPitchRel  = 3;
  static constexpr int EnvDutyCycle = 4;
  static constexpr int EnvWave      = 5;
  static constexpr int EnvRepeat    = 6;

  static inline bool isUnsignedEnvelope(int i) {
    return i == EnvVolume || i == EnvDutyCycle;
  }

  SndChannel() : events(nullptr), instrument(nullptr), kind(255), state(0), noteValue(0), wait(0),
    release(0), duration(0), volume(0), phase(0), noiseState(1) {
    resetEnvelopes();
  }

  void reset(int ki, int vo, const uint16_t *ev) {
    events = ev;
    wait = 0;
    if (kind != ki) {
      instrument = nullptr;
      kind = ki;
      state = 0;
      noteValue = 0;
      release = 0;
      duration = 0;
      volume = vo;
      phase = 0;
      noiseState = 1;
      resetEnvelopes();
    }
  }

  void disable() {
    kind = 255;
  }

  bool isEnabled() {
    return kind != 255;
  }

  void resetEnvelopes() {
    for (int i = 0; i < 7; i++) {
      env[i].cursorIndex = 7; // index=7 means disabled
      env[i].ivalue = 0;
    }
    env[EnvVolume].uvalue = 255;
    if (instrument) {
      for (int en = 0; en < instrument->envelopesLength; en++) {
        const FamiEnvelope *fenv = famiEnvelope(en);
        env[fenv->kind].cursorIndex = en;
        if (isUnsignedEnvelope(fenv->kind)) {
          env[fenv->kind].uvalue = fenv->uvalues[0];
        } else {
          env[fenv->kind].ivalue = fenv->ivalues[0];
        }
      }
    }
  }

  const FamiEnvelope *famiEnvelope(int en) {
    return instrument
      ? (const FamiEnvelope *)&(((const uint8_t *)instrument)[instrument->envelopesOffset[en]])
      : nullptr;
  }

  void famiInstrument(const FamiInstrument *finst) {
    if (instrument == finst) return;
    instrument = finst;
    resetEnvelopes();
  }

  void note(int value, int rel, int dur, bool attack) {
    noteValue = value;
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

    int vol = (volume * env[EnvVolume].uvalue) >> 8;

    if (kind < 4) {
      // oscillator
      int timer;
      uint32_t dphase;
      {
        timer = SndData::noteToTimer[noteValue];
        int pitchBend = env[EnvPitchAbs].ivalue + env[EnvPitchRel].ivalue;
        timer += pitchBend * 16;
        if (timer < 0) timer = 0;
        else if (timer > 65535) timer = 65535;
        int t = timer >> 4;
        int f = timer & 15;
        int a = SndData::timerToDphase[t];
        int b = SndData::timerToDphase[t + 1];
        dphase = a + (((b - a) * f) >> 4);
        dphase = (dphase << 14) | (dphase >> 2);
      }
      int duty = env[EnvDutyCycle].uvalue;
      int wkind = SndData::waveKind(kind, duty);
      int band = SndData::waveBand(timer);
      int renderSize;
      const int16_t *waveTable = SndData::waveTable(wkind, band, &renderSize);
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
      // PCM rendering is handled in the song, so skip in here
    } else if (kind == 6) {
      int dphase = SndData::dphasePerNoisePitch[(noteValue + 1) & 15];
      if (first) {
        sndRenderNoiseSet(out, samples, vol, &phase, dphase, &noiseState);
      } else {
        sndRenderNoiseAdd(out, samples, vol, &phase, dphase, &noiseState);
      }
      first = false;
    }
    return first;
  }

  void advance() {
    duration--;
    if (duration <= 0) {
      state = 0; // note stop
      return;
    } else if (release > 0) {
      release--;
      if (release <= 0) {
        state = 2; // note release
      }
    }

    // advance envelopes
    for (int i = 0; i < 7; i++) {
      int index = env[i].cursorIndex & 7;
      if (index == 7) continue; // index=7 means disabled
      const FamiEnvelope *fenv = famiEnvelope(index);
      int cursor = env[i].cursorIndex >> 3;
      if (i == EnvPitchRel) {
        if (cursor >= fenv->valuesLength) continue;
        cursor++;
        if (cursor < fenv->valuesLength) {
          env[i].ivalue += fenv->ivalues[cursor];
        }
      } else {
        cursor++;
        if ((state == 1 && cursor >= fenv->release) || cursor >= fenv->valuesLength) {
          cursor = state == 1 ? fenv->loop : fenv->valuesLength - 1;
        }
        if (isUnsignedEnvelope(i)) {
          env[i].uvalue = fenv->uvalues[cursor];
        } else {
          env[i].ivalue = fenv->ivalues[cursor];
        }
      }
      env[i].cursorIndex = (cursor << 3) | index;
    }
  }
};

struct SndSong {
  const FamiHeader &fami;
  const FamiSong &song;
  int column;
  SndChannel channels[16];
  struct {
    int16_t ch;
    uint16_t blockSize;
    const uint8_t *blockData;
    uint32_t samplesLeft;
    uint32_t blockLeft;
    uint32_t state;
  } pcm;

  SndSong(const FamiHeader &fami, const FamiSong &song) : fami(fami), song(song) {
    pcm.ch = -1;
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

      const uint32_t *patternsOffset = (const uint32_t *)&root()[
        song.channelsOffset[ch] +
        sizeof(FamiChannel) +
        sizeof(uint16_t) * song.songLength
      ];

      channels[ch].reset(
        channel.kind,
        channel.initialVolume,
        pa < 0 ? nullptr : (const uint16_t *)&root()[patternsOffset[pa]]
      );
    }
  }

  void playPCM(int ch, int index) {
    const DpcmTable::Entry &entry = DpcmTable::entries[index];
    const uint8_t *data = entry.sample;
    const uint8_t *dataEnd = &data[entry.size];
    // parse the WAV file
    data += 12; // skip RIFF + file size + WAVE
    int flags = 0;
    while (data < dataEnd && flags != 7) {
      uint32_t chunkName = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      data += 4;
      uint32_t chunkSize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      data += 4;
      if (chunkName == 0x20746d66) { // "fmt "
        int align = data[12] | (data[13] << 8);
        pcm.blockSize = (align - 4) << 1;
        flags |= 1;
      } else if (chunkName == 0x74636166) { // "fact"
        pcm.samplesLeft = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        flags |= 2;
      } else if (chunkName == 0x61746164) { // "data"
        pcm.blockData = data;
        pcm.blockLeft = 0;
        flags |= 4;
      }
      data += chunkSize;
    }
    if (flags == 7) {
      // found all necessary data, activate the PCM file
      pcm.ch = ch;
    }
  }

  void renderPCM(int16_t *out, int samples, bool first) {
    constexpr int volume = 73;
    int renderLeft = samples < pcm.samplesLeft ? samples : pcm.samplesLeft;
    pcm.samplesLeft -= renderLeft;
    if (first) {
      int zeroLeft = samples - renderLeft;
      while (renderLeft > 0) {
        if (pcm.blockLeft == 0) {
          // next block
          pcm.state = sndAdpcmStateFromHeader(pcm.blockData);
          pcm.blockData += 4;
          pcm.blockLeft = pcm.blockSize;
          int16_t s = sndAdpcmFirstSample(pcm.state);
          *out++ = (s * volume) >> 8; // SET
          renderLeft--;
        } else {
          int amount = pcm.blockLeft < renderLeft ? pcm.blockLeft : renderLeft;
          sndRenderAdpcmSet(out, amount, volume, &pcm.state, &pcm.blockData);
          // sndRenderAdpcm will advance state and blockData
          out += amount;
          pcm.blockLeft -= amount;
          renderLeft -= amount;
        }
      }
      while (zeroLeft > 0) {
        *out++ = 0;
        zeroLeft--;
      }
    } else {
      while (renderLeft > 0) {
        if (pcm.blockLeft == 0) {
          // next block
          pcm.state = sndAdpcmStateFromHeader(pcm.blockData);
          pcm.blockData += 4;
          pcm.blockLeft = pcm.blockSize;
          int16_t s = sndAdpcmFirstSample(pcm.state);
          *out++ += (s * volume) >> 8; // ADD
          renderLeft--;
        } else {
          int amount = pcm.blockLeft < renderLeft ? pcm.blockLeft : renderLeft;
          sndRenderAdpcmAdd(out, amount, volume, &pcm.state, &pcm.blockData);
          // sndRenderAdpcm will advance state and blockData
          out += amount;
          pcm.blockLeft -= amount;
          renderLeft -= amount;
        }
      }
    }
  }

  void tick(int16_t *out, int samples) {
    bool patternEnd = false;
    for (int ch = 0; ch < 16; ch++) {
      SndChannel &channel = channels[ch];
      if (!channel.isEnabled() || !channel.events) continue;
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
          bool autowait = (ev & 0x0080) != 0;
          int duration = e2 & 0x7ff;
          if (note < 0x6c) { // regular note
            // 0NNNNNNNWALLLLLL
            // HHHHHDDDDDDDDDDD
            // regular note
            bool attack = (ev & 0x0040) != 0;
            int release = (ev & 0x3f) | ((e2 >> 5) & 0x7c0);
            channel.note(note, release, duration, attack);
            //log("[%d] NOTE %d/%d/%d %s%s\n",
            //  ch, note, release, duration, attack ? "A" : "x", autowait ? "W" : "x");
          } else if (note == 0x6c) { // PCM
            // 0NNNNNNNWIIIIIII
            // IIIIIDDDDDDDDDDD
            channel.note(0, duration, duration, true);
            playPCM(ch, (ev & 0x7f) | ((e2 >> 4) & 0xf80));
          }
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
            case 0x01:
              if (param == 0) { // PATEND
                patternEnd = true;
                goto next_channel;
              } else if (param == 1) { // NOINST
                channel.famiInstrument(nullptr);
              } else if (param == 2) { // STOP
                channel.state = 0;
              }
              break;
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
      if (ch == pcm.ch) {
        // render PCM channel
        if (!channel.state || pcm.samplesLeft <= 0) {
          // note ended or PCM sample ended
          channel.state = 0;
          pcm.ch = -1;
          continue;
        }
        renderPCM(out, samples, first);
        first = false;
      } else {
        first = channel.render(out, samples, first);
      }
      channel.advance();
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

  const uint8_t *fami = dataSongsBasic; // Outro Basic
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
  return 0;
}
#endif

// SPDX-License-Identifier: 0BSD
//
// GBA sound engine based on wavetable synthesis, specifically targeting songs composed with
// FamiStudio (https://famistudio.org/).
//
// Supports:
//
// 1. FamiStudio Text import with high compatibility
// 2. Original NES voices (square, triangle, noise, DPCM)
// 3. VRC6 expansion (additional square and saw channels)
// 4. N163 expansion (custom wavetables) [not implemented yet -- planned]
// 5. 32768hz sample rate (targeting same rate as GBA hardware)
// 6. IM ADPCM compressed WAV files for use with sound effects and DPCM voice
// 7. DPCM sample swapping, so higher quality WAV files can be used instead of crappy NES samples
//
// NOTE: some features may be missing, but they will be implemented (eventually :-P)
//
// Usage:
//
// // declare a global Snd object:
// Snd snd(maxSongs, maxSfxs); // choose how many songs and sfxs slots you want; snd(2, 4) is good
//
// // after every vblank, run snd.copy():
// Swi::vblankIntrWait();
// snd.copy();
//
// // setting volume
// snd.masterVolume = A; // 0-16 (or more, if you really want)
// snd.songVolume = B;   // 0-16 (or more, if you really want)
// snd.sfxVolume = C;    // 0-16 (or more, if you really want)
// // if masterVolume is 0, then all processing is skipped (cart is not accessed)
//
// // playing songs:
// snd.loadSong(0, dataSongsFoo, 0);
// //           |  |             ^- which song in the file? files can contain multiple songs
// //           |  +- binary data output from scripts/famistudio.ts
// //           +- which song slot? 0...maxSongs-1
//
// // playing sound effects:
// #include "data/sfxs/Sfx.hpp"
// snd.playSfx(Sfx::foo, Sfx::fooSize, priority);
// // where lower `priority` values can override higher `priority` values if needed
//
#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef PLATFORM_GBA
// on the GBA, we render at 12 bits per sample, so we have 4 bits headroom for clipping before
// overflow wraps
#define MOONPAK_SAMPLE_SHIFT  12
#else
// on the host, we render at 16 bits per sample, and check for clipping on every add (too expensive
// on the GBA)
#define MOONPAK_SAMPLE_SHIFT  8
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
  int16_t loopChannelsLength; // loop:12, channelsLength:4
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

  void disable() {
    kind = 255;
  }

  bool isEnabled() {
    return kind != 255;
  }

  void reset(int ki, int vo, const uint16_t *ev);
  void resetEnvelopes();
  void setInstrument(const FamiInstrument *finst);
  void setNote(int value, int rel, int dur, bool attack);
  bool render(int16_t *out, int samples, int songVolume, bool first);
  void advance();
};

struct SndPCM {
  union {
    int16_t ch; // inside a song, we need to track DPCM channel
    int16_t priority; // inside sfx engine, we need to track priority
  };
  uint16_t blockSize;
  const uint8_t *blockData;
  uint32_t samplesLeft;
  uint32_t blockLeft;
  uint32_t state;

  SndPCM() { reset(); }
  void reset() { samplesLeft = 0; }
  bool isEnabled() { return samplesLeft > 0; }
  bool loadWav(const uint8_t *data, uint32_t size);
  bool render(int16_t *out, uint32_t samples, int songVolume, bool first);
};

struct SndSong {
  const FamiHeader *fami;
  const FamiSong *song;
  SndChannel channels[16];
  int16_t column;
  int16_t loopsLeft;
  SndPCM pcm;

  SndSong() { reset(); }
  void reset();
  const uint8_t *root() { return (const uint8_t *)fami; }
  void loadSong(const FamiHeader *fami, const FamiSong *song);
  bool isEnabled() { return fami != nullptr; }
  bool isDone();
  void loadColumn(int col);
  void playPCM(int ch, int index);
  bool tick(int16_t *out, int samples, int songVolume, bool first);
};

struct Snd {
  static Snd *global;
#ifdef PLATFORM_GBA
  static constexpr int bufferDMACount = 2;
  int8_t bufferDMA[608 * bufferDMACount];
  uint16_t bufferState;
  uint16_t bufferIndex;
#endif
  int16_t bufferTemp[552];
  SndSong *songs;
  SndPCM *sfxs;
  uint8_t frameCount;
  int8_t masterVolume; // 0-16
  int8_t sfxVolume;    // 0-16
  int8_t songVolume;   // 0-16
  uint8_t maxSongs;
  uint8_t maxSfxs;

  Snd &reset();
  Snd(uint8_t maxSongs, uint8_t maxSfxs) : frameCount(0), masterVolume(16), sfxVolume(16),
    songVolume(16), maxSongs(maxSongs), maxSfxs(maxSfxs) {
    Snd::global = this;
    songs = (SndSong *)malloc(sizeof(SndSong) * maxSongs);
    sfxs = (SndPCM *)malloc(sizeof(SndPCM) * maxSfxs);
#ifdef PLATFORM_GBA
    bufferState = 0xff; // flag that we need to run init()
#endif
    reset();
  }
  ~Snd() { free(songs); free(sfxs); }
  bool isDone();
  Snd &loadSong(int targetIndex, const uint8_t *data, int songIndex);
  Snd &setSongLoopsLeft(int targetIndex, int loopsLeft) {
    songs[targetIndex].loopsLeft = loopsLeft;
    return *this;
  }
  bool isSongLooping(int targetIndex) {
    return songs[targetIndex].song->loopChannelsLength >= 0;
  }
  bool isSongDone(int targetIndex) { return songs[targetIndex].isDone(); }
  bool playSfx(const uint8_t *data, uint32_t size, int16_t priority);
  uint32_t tick(); // returns how many samples were written

#ifdef PLATFORM_GBA
  void init();
  void copy();
  static void timer1Handler();
#endif

#ifdef TESTS
  static int test(bool verbose);
#endif
};

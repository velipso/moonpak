// SPDX-License-Identifier: 0BSD
#include "Snd.hpp"
#include "Snd.iwram.hpp"
#include "SndData.hpp"
#include "data/dpcm/DpcmTable.hpp"
#include <stdlib.h>

#ifdef PLATFORM_GBA
#include "gba/Irq.hpp"
#include "gba/Reg.hpp"
#endif

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

//
// SndChannel
//

const FamiEnvelope *famiEnvelope(const FamiInstrument *instrument, int en) {
  return instrument
    ? (const FamiEnvelope *)&(((const uint8_t *)instrument)[instrument->envelopesOffset[en]])
    : nullptr;
}

void SndChannel::reset(int ki, int vo, const uint16_t *ev) {
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

void SndChannel::resetEnvelopes() {
  for (int i = 0; i < 7; i++) {
    env[i].cursorIndex = 7; // index=7 means disabled
    env[i].ivalue = 0;
  }
  env[EnvVolume].uvalue = 255;
  if (instrument) {
    for (int en = 0; en < instrument->envelopesLength; en++) {
      const FamiEnvelope *fenv = famiEnvelope(instrument, en);
      env[fenv->kind].cursorIndex = en;
      if (isUnsignedEnvelope(fenv->kind)) {
        env[fenv->kind].uvalue = fenv->uvalues[0];
      } else {
        env[fenv->kind].ivalue = fenv->ivalues[0];
      }
    }
  }
}

void SndChannel::setInstrument(const FamiInstrument *finst) {
  if (instrument == finst) return;
  instrument = finst;
  resetEnvelopes();
}

void SndChannel::setNote(int value, int rel, int dur, bool attack) {
  noteValue = value;
  release = rel;
  duration = dur;
  state = 1; // note on
  if (attack) {
    phase = 0;
    resetEnvelopes();
  }
}

bool SndChannel::render(int16_t *out, int samples, int songVolume, bool first) {
  if (!state) return first;

  int vol = (volume * env[EnvVolume].uvalue * songVolume) >> 16;

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

void SndChannel::advance() {
  if (!state) return;

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
    const FamiEnvelope *fenv = famiEnvelope(instrument, index);
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

//
// SndPCM
//

bool SndPCM::render(int16_t *out, uint32_t samples, int pcmVolume, bool first) {
  uint32_t renderLeft = samples < samplesLeft ? samples : samplesLeft;
  if (renderLeft <= 0) return first;
  int volume = 73; // chosen to roughly match FamiStudio DPCM channel
  volume = (volume * pcmVolume) >> 8;
  samplesLeft -= renderLeft;
  if (first) {
    int zeroLeft = samples - renderLeft;
    while (renderLeft > 0) {
      if (blockLeft == 0) {
        // next block
        state = sndAdpcmStateFromHeader(blockData);
        blockData += 4;
        blockLeft = blockSize;
        int16_t s = sndAdpcmFirstSample(state);
        *out++ = (s * volume) >> 8; // SET
        renderLeft--;
      } else {
        int amount = blockLeft < renderLeft ? blockLeft : renderLeft;
        sndRenderAdpcmSet(out, amount, volume, &state, &blockData);
        // sndRenderAdpcm will advance state and blockData
        out += amount;
        blockLeft -= amount;
        renderLeft -= amount;
      }
    }
    while (zeroLeft > 0) {
      *out++ = 0;
      zeroLeft--;
    }
  } else {
    while (renderLeft > 0) {
      if (blockLeft == 0) {
        // next block
        state = sndAdpcmStateFromHeader(blockData);
        blockData += 4;
        blockLeft = blockSize;
        int16_t s = sndAdpcmFirstSample(state);
        *out++ += (s * volume) >> 8; // ADD
        renderLeft--;
      } else {
        int amount = blockLeft < renderLeft ? blockLeft : renderLeft;
        sndRenderAdpcmAdd(out, amount, volume, &state, &blockData);
        // sndRenderAdpcm will advance state and blockData
        out += amount;
        blockLeft -= amount;
        renderLeft -= amount;
      }
    }
  }
  return false;
}

//
// SndSong
//

void SndSong::reset() {
  fami = nullptr;
  song = nullptr;
  pcm.ch = -1;
  column = -1;
  for (int ch = 0; ch < 16; ch++) {
    channels[ch].disable();
  }
}

void SndSong::loadSong(const FamiHeader *fami_, const FamiSong *song_) {
  fami = fami_;
  song = song_;
  pcm.ch = -1;
  loadColumn(0);
}

bool SndSong::isDone() {
  if (fami == nullptr) return true;
  if (column >= 0 || pcm.ch >= 0) {
    return false;
  }
  for (int ch = 0; ch < 16; ch++) {
    if (channels[ch].isEnabled() && (channels[ch].events || channels[ch].state)) {
      return false;
    }
  }
  return true;
}

void SndSong::loadColumn(int col) {
  column = col;

  int channelsLength = song->loopChannelsLength & 15;
  for (int ch = 0; ch < 16; ch++) {
    if (ch > channelsLength) { // channelsLength is off by one (intentional)
      channels[ch].disable();
      continue;
    }

    const FamiChannel &channel = *(const FamiChannel *)&root()[song->channelsOffset[ch]];
    int pa = column < 0 ? -1 : channel.instances[column];

    uint32_t patternsOffsetPos =
      song->channelsOffset[ch] +
      sizeof(FamiChannel) +
      sizeof(uint16_t) * song->songLength;
    patternsOffsetPos = (patternsOffsetPos + 3) & ~3;
    const uint32_t *patternsOffset = (const uint32_t *)&root()[patternsOffsetPos];

    channels[ch].reset(
      channel.kind,
      channel.initialVolume,
      pa < 0 ? nullptr : (const uint16_t *)&root()[patternsOffset[pa]]
    );
  }
}

void SndSong::playPCM(int ch, int index) {
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

bool SndSong::tick(int16_t *out, int samples, int songVolume, bool first) {
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
          bool attack = (ev & 0x0040) != 0;
          int release = (ev & 0x3f) | ((e2 >> 5) & 0x7c0);
          channel.setNote(note, release, duration, attack);
        } else if (note == 0x6c) { // PCM
          // 0NNNNNNNWIIIIIII
          // IIIIIDDDDDDDDDDD
          channel.setNote(0, duration, duration, true);
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
              channel.setInstrument(nullptr);
            } else if (param == 2) { // STOP
              channel.state = 0;
            }
            break;
          case 0x02: { // INST1
set_instrument:
            const uint32_t *instOffset = (const uint32_t *)&root()[fami->instrumentsOffset];
            channel.setInstrument((const FamiInstrument *)&root()[instOffset[param]]);
            break;
          }
          case 0x03: // INST2
            param += 256;
            goto set_instrument;
          case 0x04: // VOL
            channel.volume = param;
            break;
          case 0x05: // PATSKIP1
            if (column == param) {
              patternEnd = true;
              goto next_channel;
            }
            break;
          case 0x06: // PATSKIP2
            if (column == param + 256) {
              patternEnd = true;
              goto next_channel;
            }
            break;
        }
      }
    }
next_channel:;
  }

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
      pcm.render(out, samples, songVolume, first);
      first = false;
    } else {
      first = channel.render(out, samples, songVolume, first);
    }
    channel.advance();
  }

  if (patternEnd) {
    column++;
    if (column >= song->songLength) {
      column = loopsLeft == 0 ? -1 : (song->loopChannelsLength >> 4);
      if (loopsLeft > 0) {
        loopsLeft--;
      }
    }
    loadColumn(column);
  }

  return first;
}

//
// Snd
//

Snd *Snd::global = nullptr;

#ifdef PLATFORM_GBA
void Snd::init() {
  Reg::IME::set(0);

  // initialize buffer
  bufferState = 0;
  bufferIndex = 0;
  for (uint32_t i = 0; i < sizeof(bufferDMA); i++) {
    bufferDMA[i] = 0;
  }

  // enable timer1 to rotate DMA buffers
  Irq::timer1(timer1Handler);
  Reg::IE::update().timer1(1).done();

  // setup timers
  Reg::TM0CNT::set(0);
  Reg::TM1CNT::set(0);

  // setup timer0 - drives the sample rate
  // cpuHz      = 2^24
  // sampleRate = 2^15
  // timer0Wait = cpuHz / sampleRate = 2^9 = 0x200
  // timer0Res  = 1 cycle
  // timer0Wait / timer0Res = 0x200
  Reg::TM0D::set(0x10000 - 0x200);

  // setup timer1 - drives the DMA buffer cycling
  // bufferSize = 0x260
  // timer1Wait = bufferSize * timer0Wait = 311296 cycles
  // timer1Res  = 64 cycles
  // timer1Wait / timer1Res = 0x1300
  Reg::TM1D::set(0x10000 - 0x1300);

  Reg::IME::set(1);

  // turn sound chip on
  Reg::SOUNDCNT_X::write()
    .master(1)
    .done();

  // set sound to use FIFO A
  Reg::SOUNDCNT_H::write()
    .psgVolume(2) // 100%
    .dmaAVolume(1)
    .dmaARight(1)
    .dmaALeft(1)
    .dmaATimer(0)
    .dmaAReset(1)
    .done();

  // set DMA1 destination to FIFO A
  Reg::DMA1DAD::set((uint32_t)Reg::FIFO_A::addr());

  // point DMA1 to buffer1
  Reg::DMA1SAD::set((uint32_t)bufferDMA);

  // enable DMA1
  Reg::DMA1CNT_H::write()
    .destControl(2)   // fixed destination
    .sourceControl(0) // increment source
    .repeat(1)
    .word32(1)
    .timing(3) // sound FIFO
    .enable(1)
    .done();

  // start timer0
  Reg::TM0CNT::write().enable(1).done();

  // start timer1
  Reg::TM1CNT::write().prescaler(1).irq(1).enable(1).done();
}
#endif

Snd &Snd::reset() {
  for (int i = 0; i < maxSongs; i++) {
    songs[i].reset();
  }
  for (int i = 0; i < maxSfxs; i++) {
    sfxs[i].reset();
  }
  return *this;
}

bool Snd::isDone() {
  for (int i = 0; i < maxSongs; i++) {
    if (songs[i].isEnabled() && !songs[i].isDone()) return false;
  }
  return true;
}

Snd &Snd::loadSong(int targetIndex, const uint8_t *data, int songIndex) {
  const FamiHeader &header = *(const FamiHeader *)data;

  int songsOffset = header.songsOffset;
  songsOffset += songIndex * 4;
  int songOffset = *(uint32_t *)&data[songsOffset];
  const FamiSong &song = *(const FamiSong *)&data[songOffset];

  SndSong &sndSong = songs[targetIndex];
  sndSong.loadSong(&header, &song);
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

  Snd snd(1, 0);
  snd.loadSong(0, dataSongsOutro, 0);
  snd.setSongLoopsLeft(0, 0);
  while (!snd.isDone()) {
    int sampleCount = snd.tick();
    for (int i = 0; i < sampleCount; i++) {
      push(snd.bufferTemp[i]);
    }
  }

  writeWAV("temp/sndout.wav", g_out, g_outSize);
  return 0;
}
#endif

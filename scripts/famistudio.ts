// SPDX-License-Identifier: 0BSD
// @ts-expect-error -- intentionally no @types/node
import fs from 'node:fs/promises';
// @ts-expect-error -- intentionally no @types/node
import path from 'node:path';
// @ts-expect-error -- intentionally no @types/node
import { inspect } from 'node:util';
// @ts-expect-error -- intentionally no @types/node
import { spawn } from 'node:child_process';

declare const process: {
  argv: string[];
  exit(code?: number): never;
  stdout: any;
};

declare const Buffer: {
  from(buffer: ArrayBufferLike, offset: number, length: number): unknown;
};

interface Chunk {
  name: string;
  attributes: Map<string, string>;
  children: Chunk[];
  depth: number;
}

interface DpcmTableEntry {
  name: string;
  rate: number;
}

const EnvelopeKindVolume    = 0 as const;
const EnvelopeKindArpeggio  = 1 as const;
const EnvelopeKindPitchAbs  = 2 as const;
const EnvelopeKindPitchRel  = 3 as const;
const EnvelopeKindDutyCycle = 4 as const;
const EnvelopeKindWave      = 5 as const;
const EnvelopeKindRepeat    = 6 as const;

type EnvelopeKind =
  | typeof EnvelopeKindVolume
  | typeof EnvelopeKindArpeggio
  | typeof EnvelopeKindPitchAbs
  | typeof EnvelopeKindPitchRel
  | typeof EnvelopeKindDutyCycle
  | typeof EnvelopeKindWave
  | typeof EnvelopeKindRepeat;

interface Envelope {
  kind: EnvelopeKind;
  loop: number;
  release: number;
  values: number[]; // int8_t
}

interface Instrument {
  envelopes: Envelope[];
}

interface Pattern {
  events: number[];
}

const ChannelKindSine     = 0 as const;
const ChannelKindTriangle = 1 as const;
const ChannelKindSaw      = 2 as const;
const ChannelKindSquare   = 3 as const;
const ChannelKindWave     = 4 as const;
const ChannelKindPCM      = 5 as const;
const ChannelKindNoise    = 6 as const;

type ChannelKind =
  | typeof ChannelKindSine
  | typeof ChannelKindSquare
  | typeof ChannelKindTriangle
  | typeof ChannelKindSaw
  | typeof ChannelKindWave
  | typeof ChannelKindPCM
  | typeof ChannelKindNoise;

interface Channel {
  kind: ChannelKind;
  initialVolume: number;
  patterns: Pattern[];
  instances: number[];
}

interface Song {
  length: number;
  loop: number;
  channels: Channel[];
}

interface SongFile {
  instruments: Instrument[];
  songs: Song[];
}

function envelopeKind(type: string | undefined, relative: string | undefined): EnvelopeKind | null {
  switch (type) {
    case 'Volume': return EnvelopeKindVolume;
    case 'Arpeggio': return EnvelopeKindArpeggio;
    case 'Pitch': return relative ? EnvelopeKindPitchRel : EnvelopeKindPitchAbs;
    case 'DutyCycle': return EnvelopeKindDutyCycle;
    case 'N163Wave': return EnvelopeKindWave;
    case 'Repeat': return EnvelopeKindRepeat;
  }
  return null;
}

class ChannelInfo {
  kind: ChannelKind;
  octaveOffset: number;
  dutyType: number;
  volumeScale: number[] | number;
  hash: string;

  constructor(
    kind: ChannelKind,
    octaveOffset: number,
    dutyType: number,
    volumeScale: number[] | number
  ) {
    this.kind = kind;
    this.octaveOffset = octaveOffset;
    this.dutyType = dutyType;
    this.volumeScale = volumeScale;
    this.hash = JSON.stringify([
      this.kind,
      this.octaveOffset,
      this.dutyType,
      this.volumeScale
    ]);
  }

  initialVolume() {
    return this.volume(15, 'Quarter');
  }

  volume(i: number, volumeMapping: string) {
    switch (volumeMapping) {
      case 'Quarter': break; // do nothing
      case 'Half': i = Math.floor(i * 31 / 15); break;
      case 'Full': i = Math.floor(i * 63 / 15); break;
      default: throw new Error(`Invalid volume mapping: ${volumeMapping}`);
    }
    const clamp = (num: number) =>
      Math.max(0, Math.min(255, Math.floor(num * 256))); // TODO: pick the right scale
    if (typeof this.volumeScale === 'number') {
      return clamp(i * this.volumeScale);
    } else {
      if (i < 0 || i >= this.volumeScale.length) {
        throw new Error(`Outside volume scale: ${i}`);
      }
      return clamp(this.volumeScale[i]);
    }
  }
}

// for each channel type, a FamiStudio test song was exported that plays the same sustained note at
// volumes 0..15 -- the values below are linear gain factors:
//
//   gain[v] = famistudioRms[v] / referenceWaveRms
//
// famistudioRms[v] - steady-state RMS at each volume, excluding note-start/end transients
// referenceWaveRms - RMS of the wave table oscillators
const chSQA = new ChannelInfo(
  ChannelKindSquare, 1, 1, [
  0, 0.018338, 0.036240, 0.053715, 0.070779, 0.087445, 0.103731, 0.119647,
  0.135207, 0.150420, 0.165302, 0.179855, 0.194098, 0.208034, 0.221679, 0.235040
]);
const chSQV = new ChannelInfo(ChannelKindSquare, 1, 2, 0.0178415);
const chTRI = new ChannelInfo(
  ChannelKindTriangle, 0, 0, [
  0, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903,
  0.38903, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903, 0.38903
]);
const chNOI = new ChannelInfo(
  ChannelKindNoise, 0, 0, [
  0, 0.004444, 0.008832, 0.013124, 0.017366, 0.021586, 0.025794, 0.029954,
  0.033954, 0.037931, 0.041828, 0.045823, 0.049695, 0.053626, 0.057171, 0.060915
]);
const chPCM = new ChannelInfo(
  ChannelKindPCM, 0, 0, [
  0, 10, 20, 30, 40, 50, 60, 70,
  80, 90, 100, 110, 120, 130, 140, 150
]);
const chSAW = new ChannelInfo(ChannelKindSaw, 1, 0, 0.014365);
const chWAV = new ChannelInfo(ChannelKindWave, 1, 0, 0.10537);

function famiChannelInfo(type: string | undefined): ChannelInfo | null {
  switch (type) {
    case 'Square1':
    case 'Square2':
      return chSQA;
    case 'VRC6Square1':
    case 'VRC6Square2':
      return chSQV;
    case 'Triangle':
      return chTRI;
    case 'Noise':
      return chNOI;
    case 'DPCM':
      return chPCM;
    case 'VRC6Saw':
      return chSAW;
    case 'N163Wave1':
    case 'N163Wave2':
    case 'N163Wave3':
    case 'N163Wave4':
    case 'N163Wave5':
    case 'N163Wave6':
    case 'N163Wave7':
    case 'N163Wave8':
      return chWAV;
  }
  return null;
}

function parseNote(note: string | undefined, octaveOffset: number): number {
  if (!note) {
    return -1;
  }
  const m = note.match(/^([A-G]#?)([0-9])$/);
  if (!m) {
    return -1;
  }
  const oct = parseFloat(m[2]) + octaveOffset;
  const notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const n = notes.indexOf(m[1]);
  if (n < 0) {
    return -1;
  }
  return oct * 12 + n;
}

const EV_NOTE   = 0x0000;
const EV_PCM    = 0x6c00;
const EV_WAIT   = 0x8000;
const EV_PATEND = 0x8100;
const EV_NOINST = 0x8101;
const EV_STOP   = 0x8102;
const EV_INST1  = 0x8200;
const EV_INST2  = 0x8300;
const EV_VOL    = 0x8400;

interface PatternEvent {
  frame: number;
  bias: number;
  wait?: number;
  value: number[];
  index: number;
}

class Events {
  length: number;
  channelInfo: ChannelInfo;
  volumeMapping: string;
  lastVolume: number;
  initialEventsSize: number;
  events: PatternEvent[];

  constructor(length: number, channelInfo: ChannelInfo) {
    this.length = length;
    this.channelInfo = channelInfo;
    this.volumeMapping = 'Quarter';
    this.lastVolume = 15;
    this.events = [];
    this.push({
      frame: length - 1,
      bias: 9999,
      value: [EV_PATEND]
    });
    this.initialEventsSize = this.events.length;
  }

  push(ev: Omit<PatternEvent, 'index'>) {
    if (!Number.isInteger(ev.frame) || ev.frame < 0 || ev.frame >= this.length) {
      throw new Error('Invalid frame');
    }
    this.events.push({ ...ev, index: this.events.length });
  }

  pcm(
    frame: number,
    index: number,
    duration: number
  ) {
    if (!Number.isInteger(index) || index < 0 || index >= 4096) {
      throw new Error('Invalid PCM index');
    }
    if (!Number.isInteger(duration) || duration < 0 || duration >= 2048) {
      throw new Error('Invalid duration');
    }
    this.push({
      frame,
      bias: 999,
      wait: duration,
      // 0NNNNNNNWIIIIIII
      // IIIIIDDDDDDDDDDD
      // 0x80 = advance by duration (if possible)
      value: [
        EV_PCM | 0x0080 | (index & 0x7f),
        (((index >> 7) & 0x1f) << 11) | duration
      ]
    });
  }

  stopNote(frame: number) {
    this.push({
      frame,
      bias: 998,
      value: [EV_STOP]
    });
  }

  note(
    frame: number,
    note: number,
    duration: number,
    release: number,
    attack: boolean
  ) {
    if (!Number.isInteger(note) || note < 0 || note >= 108) {
      throw new Error('Invalid note');
    }
    if (!Number.isInteger(duration) || duration < 0 || duration >= 2048) {
      throw new Error('Invalid duration');
    }
    if (!Number.isInteger(release) || release < 0) {
      throw new Error('Invalid release');
    }
    release = Math.min(duration, release);
    this.push({
      frame,
      bias: 999,
      wait: duration,
      // 0NNNNNNNWALLLLLL
      // HHHHHDDDDDDDDDDD
      // 0x80 = advance by duration (if possible)
      // 0x40 = attack exists
      value: [
        EV_NOTE | (note << 8) | 0x0080 | (attack ? 0x0040 : 0) | (release & 0x3f),
        (((release >> 6) & 0x1f) << 11) | duration
      ]
    });
  }

  setVolumeMapping(frame: number, volumeMapping: string) {
    if (this.volumeMapping !== volumeMapping) {
      this.volumeMapping = volumeMapping;
      this.volume(frame, this.lastVolume);
    }
  }

  instrument(frame: number, instrument: number) {
    if (!Number.isInteger(instrument)) {
      throw new Error('Invalid instrument index');
    }
    if (instrument < 0) {
      this.push({ frame, bias: 0, value: [EV_NOINST] });
    } else if (instrument < 256) {
      this.push({ frame, bias: 0, value: [EV_INST1 | instrument] });
    } else if (instrument < 512) {
      this.push({ frame, bias: 0, value: [EV_INST2 | (instrument - 256)] });
    } else {
      console.error('Too many instruments; max of 512');
      process.exit(1);
    }
  }

  volume(frame: number, volume: number) {
    if (!Number.isInteger(volume) || volume < 0 || volume > 15) {
      throw new Error(`Unexpected volume ${volume} (expecting 0-15)`);
    }
    this.lastVolume = volume;
    volume = this.channelInfo.volume(volume, this.volumeMapping);
    this.push({
      frame,
      bias: 1,
      value: [EV_VOL | volume]
    });
  }

  done(force = false): number[] | false {
    if (!force && this.events.length <= this.initialEventsSize) {
      return false; // empty events
    }

    // sort events by frame + bias
    this.events.sort((a, b) => {
      const v1 = a.frame - b.frame;
      if (v1 !== 0) return v1;
      const v2 = a.bias - b.bias;
      if (v2 !== 0) return v2;
      return a.index - b.index;
    });

    // remove automatic advancing by duration if events are in the way
    for (let i = 0; i < this.events.length; i++) {
      const e = this.events[i];
      if ((e.value[0] & 0x8000) == 0 && 'wait' in e && typeof e.wait === 'number') {
        // found double-payload event... is the next event before duration?
        if (
          i < this.events.length - 1 &&
          this.events[i + 1].frame < e.frame + e.wait // next event is in the way?
        ) {
          // then we can't advance by duration :(
          delete e.wait;
          e.value[0] &= 0xff7f; // remove 0x0080 flag
        }
      }
    }

    // insert wait commands to space out events properly
    let lastFrame = 0;
    const result: number[] = [];
    for (const e of this.events) {
      let wait = e.frame - lastFrame;
      if (wait < 0) throw new Error('Bad event order?');
      while (wait > 0) {
        const w = Math.min(256, wait);
        result.push(EV_WAIT | (w - 1));
        wait -= w;
      }
      for (const v of e.value) {
        result.push(v);
      }
      lastFrame = e.frame + ('wait' in e && typeof e.wait === 'number' ? e.wait : 0);
    }
    return result;
  }
}

function printUsage(error?: string): never {
  console.log(
    'Usage: node famistudio.ts <cmd> [args...]\n\n' +
    'Processes FamiStudio text exports to extract necessary data.\n\n' +
    'Commands:\n\n' +
    '* song -o <output.bin> -t <dpcm.json> <input.txt>\n\n' +
    '  Converts FamiStudio text export <input.txt> to an event stream <output.bin>\n' +
    '  that can be used by the sound engine to play songs.\n\n' +
    '  -o <output.bin>  Output file\n' +
    '  -t <dpcm.json>   Input DPCM table mapping (generated via `dpcm-table`)\n' +
    '  <input.txt>      Input file from FamiStudio export\n\n' +
    '* dpcm-table -d <dir> -t <dpcm.json> -x <exdir> -n <name> <input.txt>+\n\n' +
    '  Scans all FamiStudio text files <input.txt>+ and DPCM samples in <dir>\n' +
    '  and writes all required samples and pitch changes to <dpcm.json>, then\n' +
    '  exports the resampled WAV files to the export directory <exdir>, and\n' +
    '  generates the HPP/CPP files <exdir>/<name>.hpp and <exdir>/<name>.cpp.\n\n' +
    '  -d <dir>         Input directory of DPCM .WAV files\n' +
    '  -t <dpcm.json>   Output DPCM table mapping\n' +
    '  -x <exdir>       Export directory for HPP/CPP/WAV files\n' +
    '  -n <name>        Name of HPP/CPP files (default: DpcmTable)\n' +
    '                   (output as: <exdir>/<name>.hpp and <exdir>/<name>.cpp)\n' +
    '  <input.txt>+     All the input songs in the project\n\n' +
    '* dpcm-export -d <dir> [-f] <input.txt>\n\n' +
    '  Extracts DPCM samples from FamiStudio text export (input.txt) and writes\n' +
    '  resulting .WAV files to directory <dir>.\n\n' +
    '  -d <dir>         Output directory for DPCM .WAV files\n' +
    '  -f               Overwrite files if they already exist\n' +
    '                   (default: skip files if they already exist)\n' +
    '  <input.txt>      Input file from FamiStudio export'
  );
  if (error) {
    console.error('\nError: %s', error);
  }
  process.exit(error ? 1 : 0);
}

interface SongArgs {
  inputFile: string;
  dpcmTableFile: string;
  outputFile: string | null;
}

interface DpcmTableArgs {
  inputFiles: string[];
  dpcmDir: string;
  exportDir: string;
  outputName: string;
  dpcmTableFile: string | null;
}

interface DpcmExportArgs {
  inputFile: string;
  dpcmDir: string;
  overwrite: boolean;
}

function parseArgs():
  | ['song', SongArgs]
  | ['dpcm-table', DpcmTableArgs]
  | ['dpcm-export', DpcmExportArgs]
{
  const args = process.argv.slice(2);
  if (args.length <= 0) {
    printUsage();
  }

  const cmd = args.shift();
  let dpcmDir: string | null = null;
  let dpcmTableFile: string | null = null;
  let exportDir: string | null = null;
  let outputName: string | null = null;
  let outputFile: string | null = null;
  let inputFiles: string[] = [];
  let overwrite = false;

  for (let i = 0; i < args.length; i++) {
    if (cmd === 'song' && args[i] === '-o') {
      i++;
      if (i >= args.length) {
        printUsage('Missing output file after -o');
      }
      if (typeof outputFile === 'string') {
        printUsage('Cannot specify multiple output files');
      }
      outputFile = args[i];
    } else if ((cmd === 'song' || cmd === 'dpcm-table') && args[i] === '-t') {
      i++;
      if (i >= args.length) {
        printUsage('Missing DPCM table file after -t');
      }
      if (typeof dpcmTableFile === 'string') {
        printUsage('Cannot specify multiple DPCM table files');
      }
      dpcmTableFile = args[i];
    } else if ((cmd === 'dpcm-table' || cmd === 'dpcm-export') && args[i] === '-d') {
      i++;
      if (i >= args.length) {
        printUsage('Missing DPCM directory after -d');
      }
      if (typeof dpcmDir === 'string') {
        printUsage('Cannot specify multiple DPCM directories');
      }
      dpcmDir = args[i];
    } else if (cmd === 'dpcm-table' && args[i] === '-x') {
      i++;
      if (i >= args.length) {
        printUsage('Missing export directory after -x');
      }
      if (typeof exportDir === 'string') {
        printUsage('Cannot specify multiple export directories');
      }
      exportDir = args[i];
    } else if (cmd === 'dpcm-table' && args[i] === '-n') {
      i++;
      if (i >= args.length) {
        printUsage('Missing output name after -n');
      }
      if (typeof outputName === 'string') {
        printUsage('Cannot specify multiple output names');
      }
      outputName = args[i];
    } else if (cmd === 'dpcm-export' && args[i] === '-f') {
      overwrite = true;
    } else {
      inputFiles.push(args[i]);
    }
  }

  if (inputFiles.length <= 0) {
    printUsage('Missing input file');
  }
  if ((cmd === 'song' || cmd === 'dpcm-export') && inputFiles.length > 1) {
    printUsage("Can't specify more than one input file");
  }
  if ((cmd === 'dpcm-table' || cmd === 'dpcm-export') && dpcmDir === null) {
    printUsage('Missing DPCM directory -d');
  }

  if (cmd === 'song') {
    if (!dpcmTableFile) {
      printUsage('Missing DPCM table file -t');
    }
    return ['song', { inputFile: inputFiles[0], dpcmTableFile, outputFile }];
  } else if (cmd === 'dpcm-table') {
    if (dpcmDir === null) throw new Error('Bad dpcmDir');
    if (exportDir === null) {
      printUsage('Missing export directory -x');
    }
    if (outputName === null) {
      outputName = 'DpcmTable';
    }
    return ['dpcm-table', { inputFiles, dpcmDir, exportDir, outputName, dpcmTableFile }];
  } else if (cmd === 'dpcm-export') {
    if (dpcmDir === null) throw new Error('Bad dpcmDir');
    return ['dpcm-export', { inputFile: inputFiles[0], dpcmDir, overwrite }];
  } else {
    printUsage(`Invalid command: ${cmd}`);
  }
}

function parseLine(line: string): { name: string; attributes: Map<string, string> } {
  let name = '';
  const attributes = new Map<string, string>();
  let state = 0;
  let key = '';
  let value = '';
  for (let i = 0; i < line.length; i++) {
    const ch = line.charAt(i);
    const nch = line.charAt(i + 1);
    switch (state) {
      case 0: // read name
        if (ch === ' ') {
          key = '';
          state = 1;
        } else {
          name += ch;
        }
        break;
      case 1: // read key
        if (ch === '=') {
          value = '';
          state = 2;
        } else if (ch !== ' ') {
          key += ch;
        }
        break;
      case 2: // read "value"
        if (ch === '"') {
          state = 3;
        } else {
          throw new Error('Invalid FamiStudio attribute value');
        }
        break;
      case 3: // read value
        if (ch === '"' && nch === '"') {
          value += '"';
          i++;
        } else if (ch === '"') {
          attributes.set(key, value);
          key = '';
          state = 1;
        } else {
          value += ch;
        }
        break;
    }
  }
  return { name, attributes };
}

function parseFileIntoTree(fileData: string): Chunk[] {
  const lines = fileData.split('\n');
  const root: Chunk = {
    name: 'Root',
    attributes: new Map(),
    children: [],
    depth: -1
  };
  const here = [root];
  for (let line of lines) {
    if (!line.trim()) continue;
    let tab = 0;
    while (line.charAt(tab) === ' ' || line.charAt(tab) === '\t') {
      tab++;
    }
    const { name, attributes } = parseLine(line.substr(tab));
    const chunk: Chunk = {
      name,
      attributes,
      children: [],
      depth: tab
    };
    while (tab <= here[0].depth) {
      here.shift();
    }
    while (tab > here[0].depth + 1) {
      here.unshift(here[0].children[here[0].children.length - 1]);
    }
    here[0].children.push(chunk);
  }
  return root.children;
}

function parseTreeIntoSong(
  rootChildren: Chunk[],
  dpcmTable: DpcmTableEntry[],
  writeDpcmTable: boolean
): SongFile {
  const out: SongFile = {
    instruments: [],
    songs: [],
  };

  const project = rootChildren.find(c => c.name === 'Project');
  if (!project) {
    throw new Error('Missing "Project"');
  }

  // validate project
  const tempoMode = project.attributes.get('TempoMode');
  if (tempoMode !== 'FamiStudio') {
    console.error(`Unsupported tempo mode "${tempoMode}"; only "FamiStudio" supported`);
    process.exit(1);
  }
  const expansions = project.attributes.get('Expansions')?.split(',') ?? [];
  for (const e of expansions) {
    if (e !== 'VRC6' && e !== 'N163') {
      console.error(`Unsupported expansion "${e}"; only "VRC6" and "N163" supported`);
      process.exit(1);
    }
  }

  const instrumentByName = new Map(
    project.children.filter(c => c.name === 'Instrument').map(c => [
      c.attributes.get('Name') ?? '',
      c
    ])
  );
  const instrumentNameToIndex = new Map<string, number>();
  function findInstrument(
    name: string,
    channelInfo: ChannelInfo
  ): { index: number; volumeMapping: string } | null {
    if (name === '') return null;
    // parse envelope-based instruments
    const instrument = instrumentByName.get(name);
    if (!instrument) {
      throw new Error(`Missing instrument: ${name}`);
    }
    const volumeMapping = channelInfo.kind === ChannelKindSaw
      ? instrument.attributes.get('Vrc6SawMasterVolume') ?? 'Full'
      : 'Quarter';

    // TODO: convert N163Wave presets to regular instruments if possible
    // Sine, Triangle, Sawtooth, Square50%, Square25%
    // allows for triangle with volume! :)

    // check cache first
    const key = `${name}\0${channelInfo.hash}`;
    const cacheIndex = instrumentNameToIndex.get(key);
    if (typeof cacheIndex === 'number') {
      return { index: cacheIndex, volumeMapping };
    }

    const envelopes = instrument.children.filter(c => c.name === 'Envelope');
    if (envelopes.length <= 0) {
      return { index: -1, volumeMapping };
    }

    const envs: Envelope[] = [];
    for (const envelope of envelopes) {
      const attr = envelope.attributes;
      const kind = envelopeKind(attr.get('Type'), attr.get('Relative'));
      if (kind === null) {
        console.error(envelope);
        throw new Error('Invalid envelope');
      }
      if (kind === EnvelopeKindDutyCycle && channelInfo.dutyType === 0) continue;
      const loop = parseFloat(attr.get('Loop') ?? '');
      const release = parseFloat(attr.get('Release') ?? '');
      let values = (attr.get('Values')?.split(',') || []).map(parseFloat);
      if (values.length > 0) {
        if (kind === EnvelopeKindVolume) {
          values = values.map(
            v => channelInfo.kind === ChannelKindTriangle
              ? (v > 0 ? 255 : 0)
              : Math.floor(v * 255 / 15)
          );
        } else if (kind === EnvelopeKindDutyCycle && channelInfo.dutyType === 1) {
          // convert NES-style duty to VRC6-style duty
          values = values.map(v => {
            switch (v) {
              case 0: return 1;
              case 1: return 3;
              case 2: return 7;
              case 3: return 3;
            }
            throw new Error(`Unknown duty cycle envelope value: ${v}`);
          });
        }
        envs.push({
          kind,
          loop: isNaN(loop) ? values.length - 1 : loop,
          release: isNaN(release) ? values.length : release,
          values
        });
      }
    }

    const index = out.instruments.length;
    instrumentNameToIndex.set(key, index);
    out.instruments.push({ envelopes: envs });
    return { index, volumeMapping };
  }

  const instrumentPCMToMap = new Map<string, Map<string, DpcmTableEntry>>();
  function findInstrumentPCM(name: string): Map<string, DpcmTableEntry> | null {
    if (name === '') return null;

    const cache = instrumentPCMToMap.get(name);
    if (cache) return cache;

    // parse DPCM-based instruments
    const instrument = instrumentByName.get(name);
    if (!instrument) {
      throw new Error(`Missing instrument: ${name}`);
    }
    const result = new Map<string, DpcmTableEntry>();
    const mappings = instrument.children.filter(c => c.name === 'DPCMMapping');
    for (const mapping of mappings) {
      const note = mapping.attributes.get('Note');
      const name = dpcmName(mapping.attributes.get('Sample') ?? '');
      const rate = parseFloat(mapping.attributes.get('Pitch') ?? '');
      if (note && name && Number.isInteger(rate) && rate >= 0 && rate < 16) {
        if (mapping.attributes.get('Loop') === 'True') {
          console.error(`WARNING: Sound engine doesn't support DPCM looping samples: ${name}`);
        }
        result.set(note, { name, rate });
      }
    }
    instrumentPCMToMap.set(name, result);
    return result;
  }

  // parse songs
  const songs = project.children.filter(c => c.name === 'Song');
  for (const song of songs) {
    const songLength = parseFloat(song.attributes.get('Length') ?? '');
    // TODO: implement non-looping songs
    const songLoop = Math.max(0, parseFloat(song.attributes.get('LoopPoint') ?? '0'));
    const patternLength = parseFloat(song.attributes.get('PatternLength') ?? '');
    const noteLength = parseFloat(song.attributes.get('NoteLength') ?? '');
    if (isNaN(songLength) || isNaN(patternLength) || isNaN(noteLength)) {
      throw new Error('Invalid song attributes');
    }
    const chans: Channel[] = [];
    const channels = song.children.filter(c => c.name === 'Channel');
    for (const channel of channels) {
      const channelInfo = famiChannelInfo(channel.attributes.get('Type'));
      if (!channelInfo) {
        console.error('Invalid channel:', channel);
        process.exit(1);
      }

      const patts: Pattern[] = [];
      const patterns = channel.children.filter(c => c.name === 'Pattern');
      const patternNameToIndex = new Map<string, number>();
      for (const pattern of patterns) {
        const patternName = pattern.attributes.get('Name');
        if (!patternName) {
          throw new Error('Missing pattern name');
        }
        const events = new Events(patternLength * noteLength, channelInfo);

        let lastInstrument = -999;
        const notes = pattern.children.filter(c => c.name === 'Note');
        for (const note of notes) {
          const attr = [...note.attributes.entries()];
          const getAttr = (name: string): string => {
            const i = attr.findIndex(a => a[0] === name);
            if (i < 0) {
              return '';
            }
            return attr.splice(i, 1)[0][1];
          };

          // TODO: account for groove
          const frame = parseFloat(getAttr('Time'));
          if (isNaN(frame)) {
            throw new Error('Invalid time field in pattern note');
          }

          const pcmInst = channelInfo.kind === ChannelKindPCM
            ? findInstrumentPCM(getAttr('Instrument'))
            : null;

          if (channelInfo.kind !== ChannelKindPCM) {
            const volume = parseFloat(getAttr('Volume'));
            if (!isNaN(volume)) {
              events.volume(frame, volume);
            }

            const thisInstrument = findInstrument(getAttr('Instrument'), channelInfo);
            if (thisInstrument) {
              events.setVolumeMapping(frame, thisInstrument.volumeMapping);
              if (thisInstrument.index !== lastInstrument) {
                events.instrument(frame, thisInstrument.index);
                lastInstrument = thisInstrument.index;
              }
            }
          }

          const noteStr = getAttr('Value');
          const noteVal = parseNote(noteStr, channelInfo.octaveOffset);
          if (noteVal >= 0) {
            const duration = parseFloat(getAttr('Duration'));
            if (isNaN(duration)) {
              throw new Error('Note missing Duration');
            }
            if (channelInfo.kind === ChannelKindPCM) {
              const e = pcmInst?.get(noteStr);
              if (e) {
                let index = dpcmTable.findIndex(d => d.name === e.name && d.rate === e.rate);
                if (index < 0) {
                  if (writeDpcmTable) {
                    // note: this index will actually be wrong, because the dpcmTable will be
                    // resorted later... but it doesn't matter, these events will be thrown out
                    index = dpcmTable.length;
                    dpcmTable.push(e);
                  } else {
                    console.error(`Missing DPCM sample: ${e.name} (${e.rate})`);
                    process.exit(2);
                  }
                }
                events.pcm(frame, index, duration);
              }
            } else {
              let release = parseFloat(getAttr('Release'));
              if (isNaN(release)) {
                release = duration;
              }
              const attack = getAttr('Attack') !== 'False';
              // TODO: famistudio can report Attack=False but not honor it if the channels/envelopes
              // are too different... need to mimic that logic here:
              // https://github.com/BleuBleu/FamiStudio/blob/70625dd09d8acaef831bbce468d416fb0b596be1/FamiStudio/Source/Project/Channel.cs#L1478
              events.note(frame, noteVal, duration, release, attack);
              // TODO: arpeggio
              // TODO: slide note
            }
          } else if (noteStr === 'Stop') {
            events.stopNote(frame);
          }

          // TODO: handle unknown attrs: if (attr.length > 0) console.log(attr);
        }

        const evs = events.done();
        if (evs) {
          patternNameToIndex.set(patternName, patts.length);
          patts.push({ events: evs });
        } else {
          patternNameToIndex.set(patternName, -1);
        }
      }

      const insts: number[] = [];
      for (let i = 0; i < songLength; i++) {
        insts.push(-1);
      }
      const instances = channel.children.filter(c => c.name === 'PatternInstance');
      for (const instance of instances) {
        const time = parseFloat(instance.attributes.get('Time') ?? '');
        const patternName = instance.attributes.get('Pattern') ?? '';
        const index = patternNameToIndex.get(patternName);
        if (isNaN(time) || typeof index === 'undefined') {
          throw new Error('Invalid pattern instance');
        }
        insts[time] = index;
      }

      if (insts.some(i => i >= 0)) {
        chans.push({
          kind: channelInfo.kind,
          initialVolume: channelInfo.initialVolume(),
          patterns: patts,
          instances: insts,
        });
      }
    }

    // check for completely empty columns and insert an empty pattern to take up time if needed
    let emptyPatternIndex = -1;
    for (let column = 0; column < songLength; column++) {
      if (chans.some(ch => ch.instances[column] >= 0)) {
        // a channel defines a pattern in this column, so skip
        continue;
      }
      // empty column!
      if (emptyPatternIndex < 0) {
        const ev = new Events(patternLength * noteLength, chTRI);
        const events = ev.done(true);
        if (!events) {
          throw new Error('Empty events failed to generate');
        }
        emptyPatternIndex = chans[0].patterns.length;
        chans[0].patterns.push({ events });
      }
      chans[0].instances[column] = emptyPatternIndex;
    }

    out.songs.push({
      length: songLength,
      loop: songLoop,
      channels: chans
    });
  }

  return out;
}

function serializeSong(out: SongFile): number[] {
  // serialize to bytes
  const bytes: number[] = [0x66, 0x61, 0x6d, 0x69]; // "fami"
  const write8 = (v: number) => bytes.push(v & 0xff);
  const align16 = () => { while (bytes.length & 1) write8(0); };
  const align32 = () => { while (bytes.length & 3) write8(0); };
  const write16 = (v: number) => { write8(v); write8(v >> 8); };
  const write32 = (v: number) => { write8(v); write8(v >> 8); write8(v >> 16); write8(v >> 24); };
  const rewrite8 = () => {
    const i = bytes.length;
    write8(0);
    return (v: number) => {
      bytes[i] = v & 0xff;
    };
  };
  const rewrite16 = () => {
    const i = bytes.length;
    write16(0);
    return (v: number) => {
      bytes[i] = v & 0xff;
      bytes[i + 1] = (v >> 8) & 0xff;
    };
  };
  const rewrite32 = () => {
    const i = bytes.length;
    write32(0);
    return (v: number) => {
      bytes[i] = v & 0xff;
      bytes[i + 1] = (v >> 8) & 0xff;
      bytes[i + 2] = (v >> 16) & 0xff;
      bytes[i + 3] = (v >> 24) & 0xff;
    };
  };

  write16(out.instruments.length);
  write16(out.songs.length);
  align32();
  const instrumentsOffset = rewrite32();
  const songsOffset = rewrite32();

  instrumentsOffset(bytes.length);
  const instRewrite = out.instruments.map(() => rewrite32());
  for (const instrument of out.instruments) {
    align32();
    instRewrite.shift()?.(bytes.length);
    const instrumentStart = bytes.length;
    if (instrument.envelopes.length >= 8) {
      throw new Error('Too many envelopes');
    }
    write8(instrument.envelopes.length);
    write8(0); // reserved
    write8(0); // reserved
    write8(0); // reserved
    const envRewrite = instrument.envelopes.map(() => rewrite32());
    for (const env of instrument.envelopes) {
      align32();
      envRewrite.shift()?.(bytes.length - instrumentStart);
      write8(env.kind);
      write8(0); // reserved
      write16(env.loop);
      write16(env.release);
      if (env.values.length <= 0) throw new Error('Empty envelope');
      write16(env.values.length);
      for (const v of env.values) {
        write8(v < 0 ? 256 + v : v);
      }
    }
  }

  align32();
  songsOffset(bytes.length);
  const songRewrite = out.songs.map(() => rewrite32());
  for (const song of out.songs) {
    align32();
    songRewrite.shift()?.(bytes.length);
    if (song.channels.length < 1) throw new Error('No channels');
    if (song.channels.length > 16) throw new Error('Too many channels');
    write16((song.loop << 4) | (song.channels.length - 1));
    write16(song.length);
    const chanRewrite = song.channels.map(() => rewrite32());
    for (const channel of song.channels) {
      align32();
      chanRewrite.shift()?.(bytes.length);
      write8(channel.kind);
      write8(channel.initialVolume);
      write16(channel.patterns.length);
      if (channel.instances.length !== song.length) {
        throw new Error("Instance length doesn't match song length");
      }
      for (const inst of channel.instances) {
        write16(inst);
      }
      const pattRewrite = channel.patterns.map(() => rewrite32());
      for (const pattern of channel.patterns) {
        align32();
        pattRewrite.shift()?.(bytes.length);
        for (const ev of pattern.events) {
          write16(ev);
        }
      }
    }
  }

  // end of file
  align32();
  return bytes;
}

function dpcmName(name: string) { // FamiStudio name -> internal name
  return name.replace(/[^a-zA-Z_0-9]/g, '_');
}

async function getDpcmFiles(dpcmDir: string): Promise<string[]> {
  const result: string[] = [];
  const files = await fs.readdir(dpcmDir, { withFileTypes: true });
  for (const file of files) {
    if (file.isFile()) {
      const m = file.name.match(/^(.*)\.wav/);
      if (m) {
        result.push(m[1]);
      }
    }
  }
  return result;
}

function decodeDpcm(level: number, data: number[]) {
  const levels: number[] = [];
  for (const byte of data) {
    for (let bit = 0; bit < 8; bit++) {
      if (byte & (1 << bit)) {
        if (level < 63) level++;
      } else {
        if (level > 0) level--;
      }
      levels.push(level);
    }
  }

  // trim trailing zeros, which hover around 32 due to DPCM weirdness
  let lastPop;
  while (levels.length > 0 && Math.abs(levels[levels.length - 1] - 32) <= 1) {
    lastPop = levels.pop();
  }
  // ... except save the last one
  if (typeof lastPop === 'number') {
    levels.push(lastPop);
  }

  const pcm = new Uint8Array(levels.length);
  for (let i = 0; i < levels.length; i++) {
    const level = levels[i];
    pcm[i] = (level << 2) | (level >> 4);
  }
  return pcm;
}

function writeWav(pcm: Uint8Array, filename: string) {
  return new Promise<void>((resolve) => {
    const proc = spawn('ffmpeg', [
      '-loglevel', 'fatal',
      '-f', 'u8',
      '-ar', '33144', // DPCM 15 sample rate (1789773 / 54)
      '-ac', '1',
      '-i', 'pipe:0',
      '-c:a', 'pcm_u8',
      '-y',
      filename,
    ], {
      stdio: ['pipe', 'inherit', 'inherit'],
    });
    proc.on('error', (err: unknown) => {
      console.error(err);
      process.exit(1);
    });
    proc.on('close', (code: number) => {
      if (code !== 0) {
        console.error(`ffmpeg failed (exit ${code})`);
        process.exit(1);
      }
      resolve();
    });
    proc.stdin.end(Buffer.from(pcm.buffer, pcm.byteOffset, pcm.byteLength));
  });
}

async function ffprobeSampleRate(filename: string): Promise<number> {
  return new Promise((resolve) => {
    const proc = spawn('ffprobe', [
      '-v', 'error',
      '-select_streams', 'a:0',
      '-show_entries', 'stream=sample_rate',
      '-of', 'default=noprint_wrappers=1:nokey=1',
      filename,
    ], {
      stdio: ['ignore', 'pipe', 'inherit'],
    });
    let output = '';
    proc.stdout.setEncoding('utf8');
    proc.stdout.on('data', (data: string) => output += data);
    proc.on('error', (err: unknown) => {
      console.error(err);
      process.exit(1);
    });
    proc.on('close', (code: number) => {
      if (code !== 0) {
        console.error(`ffprobe failed (exit ${code})`);
        process.exit(1);
      }
      resolve(parseFloat(output.trim()));
    });
  });
}

function convertDpcmSampleBlock(
  input: string,
  output: string,
  pitchedRate: number,
  blockSize: number
) {
  return new Promise<void>((resolve) => {
    const proc = spawn('ffmpeg', [
      '-loglevel', 'fatal',
      '-i', input,
      '-af', `asetrate=${pitchedRate},aresample=32768`,
      '-ac', '1',
      '-c:a', 'adpcm_ima_wav',
      '-block_size', `${blockSize}`,
      '-ar', '32768',
      '-y',
      output,
    ], {
      stdio: ['ignore', 'ignore', 'inherit'],
    });
    proc.on('error', (err: unknown) => {
      console.error(err);
      process.exit(1);
    });
    proc.on('close', (code: number) => {
      if (code !== 0) {
        console.error(`ffmpeg failed (exit ${code})`);
        process.exit(code);
      }
      resolve();
    });
  });
}

async function convertDpcmSample(input: string, output: string, rate: number) {
  const sourceRate = await ffprobeSampleRate(input);
  // NES DPCM periods
  const dpcmPeriods = [428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 85, 72, 54];
  const pitchedRate = Math.round(sourceRate * dpcmPeriods[15] / dpcmPeriods[rate]);
  let blockSize = 32;
  let bestBlockSize = -1;
  let bestSize = -1;
  while (blockSize <= 1024) {
    await convertDpcmSampleBlock(input, output, pitchedRate, blockSize);
    const { size } = await fs.stat(output);
    if (bestSize < 0 || size < bestSize) {
      bestBlockSize = blockSize;
      bestSize = size;
    }
    blockSize *= 2;
  }
  await convertDpcmSampleBlock(input, output, pitchedRate, bestBlockSize);
}

async function generateDpcmTableFiles(
  dpcmDir: string,
  exportDir: string,
  outputName: string,
  dpcmTable: DpcmTableEntry[]
): Promise<{ hpp: string[]; cpp: string[] }> {
  const hpp: string[] = [
    `// generated by scripts/famistudio.ts dpcm-table`,
    `#pragma once`,
    `#include <stdint.h>`,
    ``,
    `namespace DpcmTable {`,
    `  struct Entry {`,
    `    const uint8_t *sample;`,
    `    const uint32_t size;`,
    `  };`,
    ``,
    `  extern const Entry entries[];`,
    ``,
  ];
  const cpp: string[] = [
    `// generated by scripts/famistudio.ts dpcm-table`,
    `#include "${outputName}.hpp"`,
    ``,
    `namespace DpcmTable {`,
  ];
  const dpcmFiles = await getDpcmFiles(dpcmDir);
  const entries: string[] = [];
  for (const { name, rate } of dpcmTable) {
    const file = dpcmFiles.find(d => d.toLowerCase() === name.toLowerCase());
    const sourceFile = path.join(dpcmDir, `${file}.wav`);
    if (file && await fs.access(sourceFile).then(() => true, () => false)) {
      const wavName = `${name}_${rate}.wav`;
      const targetFile = path.join(exportDir, wavName);
      await convertDpcmSample(sourceFile, targetFile, rate)
      const ident = `dpcm${name.charAt(0).toUpperCase()}${name.substr(1)}_${rate}`;
      hpp.push(
        `  alignas(4) extern const uint8_t ${ident}[];`,
        `  extern const uint32_t ${ident}Size;`,
      );
      cpp.push(
        `  alignas(4) const uint8_t ${ident}[] = {`,
        `    #embed "${wavName}"`,
        `  };`,
        `  const uint32_t ${ident}Size = sizeof(${ident});`
      );
      entries.push(`    { ${ident}, sizeof(${ident}) },`);
    } else {
      console.error(`WAV missing: ${file ? sourceFile : `${dpcmDir}/${name}.wav`}`);
      process.exit(1);
    }
  }
  cpp.push(`  const Entry entries[] = {`);
  for (const e of entries) {
    cpp.push(e);
  }
  cpp.push(`    { nullptr, 0 },`);
  cpp.push(`  };`);
  hpp.push('}', '');
  cpp.push('}', '');
  return { hpp, cpp };
}

// main program
const [cmd, args] = parseArgs();
switch (cmd) {
  case 'song': {
    const { inputFile, dpcmTableFile, outputFile } = args;
    const dpcmTable = JSON.parse(await fs.readFile(dpcmTableFile, 'utf8'));
    const tree = parseFileIntoTree(await fs.readFile(inputFile, 'utf8'));
    const song = parseTreeIntoSong(tree, dpcmTable, false);
    const bytes = serializeSong(song);
    if (outputFile) {
      await fs.writeFile(outputFile, new Uint8Array(bytes));
    } else {
      for (let i = 0; i < bytes.length; i += 16) {
        console.log(bytes.slice(i, i + 16).map(v => `0${v.toString(16)}`.substr(-2)).join(' '));
      }
      console.log(bytes.length, 'bytes');
    }
    break;
  }
  case 'dpcm-table': {
    const { inputFiles, dpcmDir, exportDir, outputName, dpcmTableFile } = args;
    const dpcmTable: DpcmTableEntry[] = [];
    for (const inputFile of inputFiles) {
      const tree = parseFileIntoTree(await fs.readFile(inputFile, 'utf8'));
      parseTreeIntoSong(tree, dpcmTable, true);
    }
    dpcmTable.sort((a, b) => {
      const v1 = a.name.localeCompare(b.name);
      if (v1 !== 0) return v1;
      return a.rate - b.rate;
    });
    const { hpp, cpp } = await generateDpcmTableFiles(dpcmDir, exportDir, outputName, dpcmTable);
    await fs.writeFile(path.join(exportDir, `${outputName}.hpp`), hpp.join('\n'));
    await fs.writeFile(path.join(exportDir, `${outputName}.cpp`), cpp.join('\n'));
    if (dpcmTableFile) {
      await fs.writeFile(dpcmTableFile, JSON.stringify(dpcmTable, null, 2));
    } else {
      console.log(JSON.stringify(dpcmTable, null, 2));
    }
    break;
  }
  case 'dpcm-export': {
    const { inputFile, dpcmDir, overwrite } = args;
    const dpcmFiles = await getDpcmFiles(dpcmDir);
    const tree = parseFileIntoTree(await fs.readFile(inputFile, 'utf8'));
    const samples = tree[0].children.filter(c => c.name === 'DPCMSample');
    let sampleCount = 0;
    for (const sample of samples) {
      const name = sample.attributes.get('Name');
      if (!name) continue;
      const data = sample.attributes.get('Data');
      if (!data) continue;
      const initial = parseFloat(sample.attributes.get('DmcInitialValue') ?? '32');
      if (isNaN(initial)) {
        console.error(sample);
        throw new Error('Invalid DmcInitialValue');
      }
      const dataNum: number[] = [];
      for (let i = 0; i < data.length; i += 2) {
        dataNum.push(parseInt(data.substr(i, 2), 16));
      }
      const pcm = decodeDpcm(initial, dataNum);
      let dname = dpcmName(name);
      const exists = dpcmFiles.find(d => d.toLowerCase() === dname.toLowerCase());
      sampleCount++;
      if (exists && !overwrite) {
        console.log(`Skipping "${dname}", already exists: ${dpcmDir}/${exists}.wav`);
      } else {
        if (exists) {
          dname = exists;
        }
        dpcmFiles.push(dname);
        console.log(`${exists ? 'Overwriting' : 'Writing'} "${dname}": ${dpcmDir}/${dname}.wav`);
        await writeWav(pcm, path.join(dpcmDir, `${dname}.wav`));
      }
    }
    console.log(`Found ${sampleCount} DPCM sample${sampleCount !== 1 ? 's' : ''}: ${inputFile}`);
    break;
  }
  default:
    throw new Error(`Unknown command: ${cmd}`);
}

/*
TODO:

DPCM samples:
- initial value (DmcInitialValueDiv2) 0-63 (I think), default 32
- data (bytes)
{GenerateAttribute("Name", sample.Name)}
{ConditionalGenerateAttribute("DmcInitialValue", sample.DmcInitialValueDiv2, sample.DmcInitialValueDiv2 != 32)}
{GenerateAttribute("Data", String.Join("", sample.ProcessedData.Select(x => $"{x:x2}")))}");

Instruments:
- DPCM Mapping

Arpeggios:
- Length: 0-?
- Loop point (optional)
- Values (int8_t[] comma separated)

note octaves are incorrect (surprise)
for Triangle, it's correct, ranging from C0-B7
everything else, it's off by one, ranging from C1-B8

Songs:
- Length
- LoopPoint
- PatternLength
- BeatLength
- NoteLength
- Groove? -- must use Groove to re-time events
*/

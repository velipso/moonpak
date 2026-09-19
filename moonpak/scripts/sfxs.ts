// SPDX-License-Identifier: 0BSD
//
// Converts and generates SFX data
//
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
};

function printUsage(error?: string): never {
  console.log(
    'Usage: node sfxs.ts -o <outputDir> <inputDir>\n\n' +
    'Read all WAV files from <inputDir> and write resulting HPP/CPP/WAV output\n' +
    'files to <outputDir>.\n\n' +
    'Input files that match *.wav will be compressed using IMA ADPCM (lossy).\n' +
    'Files that match *.pcm.wav will be uncompressed.\n\n' +
    '-o <outputDir>  Output directory\n\n' +
    '<inputDir>      Input directory'
  );
  if (error) {
    console.error('\nError: %s', error);
  }
  process.exit(error ? 1 : 0);
}

function parseArgs(): { inputDir: string; outputDir: string } {
  const args = process.argv.slice(2);
  if (args.length <= 0) {
    printUsage();
  }

  let inputDir: string | null = null;
  let outputDir: string | null = null;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === '-o') {
      if (++i >= args.length) {
        printUsage('Missing output directory after -o');
      }
      if (outputDir !== null) {
        printUsage('Cannot specify multiple output directories -o');
      }
      outputDir = args[i];
    } else {
      if (inputDir !== null) {
        printUsage('Cannot specify multiple input directories');
      }
      inputDir = args[i];
    }
  }

  if (inputDir === null) {
    printUsage('Missing input directory');
  }
  if (outputDir === null) {
    printUsage('Missing output directory -o');
  }

  return { inputDir, outputDir };
}

function ffmpeg(input: string, output: string, blockSize: number | false) {
  return new Promise<void>((resolve) => {
    const proc = spawn('ffmpeg', [
      '-loglevel', 'fatal',
      '-i', input,
      '-ac', '1',
      ...(blockSize === false
        ? ['-c:a', 'pcm_s16le']
        : ['-c:a', 'adpcm_ima_wav', '-block_size', `${blockSize}`]),
      '-ar', '32768',
      '-fflags', '+bitexact',
      '-flags:a', '+bitexact',
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

const { inputDir, outputDir } = parseArgs();

const basenames: string[] = [];
for (const file of await fs.readdir(inputDir, { withFileTypes: true })) {
  if (!file.isFile()) continue;
  const m = file.name.match(/^([a-zA-Z_][a-zA-Z_0-9]*)(\.pcm)?\.wav$/);
  if (!m) {
    console.log(`Skipping: ${file.name}`);
    continue;
  }
  const basename = m[1];
  const pcm = !!m[2];
  const input = path.join(inputDir, file.name);
  const output = path.join(outputDir, `${basename}.wav`);
  basenames.push(basename);
  if (pcm) {
    console.log(`Processing: ${file.name} (pcm)`);
    await ffmpeg(input, output, false);
  } else {
    let blockSize = 32;
    let bestBlockSize = -1;
    let bestSize = -1;
    while (blockSize <= 1024) {
      await ffmpeg(input, output, blockSize);
      const { size } = await fs.stat(output);
      if (bestSize < 0 || size < bestSize) {
        bestBlockSize = blockSize;
        bestSize = size;
      }
      blockSize *= 2;
    }
    console.log(`Processing ${file.name} (block size ${bestBlockSize})`);
    await ffmpeg(input, output, bestBlockSize);
  }
}

const hpp: string[] = [
  `// generated via scripts/sfxs.ts`,
  `#pragma once`,
  `#include <stdint.h>`,
  ``,
  `namespace Sfx {`,
];
const cpp: string[] = [
  `// generated via scripts/sfxs.ts`,
  `#include "Sfx.hpp"`,
  ``,
  `namespace Sfx {`,
];
for (const basename of basenames) {
  hpp.push(
    `  alignas(4) extern const uint8_t ${basename}[];`,
    `  extern const uint32_t ${basename}Size;`,
  );
  cpp.push(
    `  alignas(4) const uint8_t ${basename}[] = {`,
    `    #embed "${basename}.wav"`,
    `  };`,
    `  const uint32_t ${basename}Size = sizeof(${basename});`,
  );
}
hpp.push(`}`, ``);
cpp.push(`}`, ``);

await Promise.all([
  fs.writeFile(path.join(outputDir, 'Sfx.hpp'), hpp.join('\n')),
  fs.writeFile(path.join(outputDir, 'Sfx.cpp'), cpp.join('\n'))
]);

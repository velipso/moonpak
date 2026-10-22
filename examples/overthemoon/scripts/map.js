// SPDX-License-Identifier: 0BSD
import path from 'path';
import fs from 'fs/promises';

function printUsage(err) {
  console.log(
    'node scripts/map.js -o output.bin input.json\n\n' +
    'Processes Tiled map file <input.json> and produces <output.bin>'
  );
  if (err) {
    console.error('\nError: %s', err);
    process.exit(1);
  }
  process.exit(0);
}

const args = process.argv.slice(2);
if (args.length <= 0) {
  printUsage();
}

let outputFile = null;
let inputFile = null;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '-o') {
    if (++i >= args.length) {
      printUsage('Missing output file after -o');
    }
    if (outputFile !== null) {
      printUsage('Cannot specify output file -o more than once');
    }
    outputFile = args[i];
  } else {
    if (inputFile !== null) {
      printUsage('Cannot specify input file more than once');
    }
    inputFile = args[i];
  }
}

if (!outputFile) {
  printUsage('Missing output file -o');
}
if (!inputFile) {
  printUsage('Missing input file');
}

const data = JSON.parse(await fs.readFile(inputFile, 'utf8'));
const out = [data.width, data.height];

const background = data.layers.find(d => d.name === 'background')?.data;
if (!background) throw new Error('Missing "background" layer');
const objects = data.layers.find(d => d.name === 'objects')?.data;
if (!objects) throw new Error('Missing "objects" layer');
const regions = data.layers.find(d => d.name === 'regions')?.objects;
if (!regions) throw new Error('Missing "regions" layer');

const tileAt = (layerData, x, y) => {
  let d = layerData[x + y * data.width];
  const flipH = (d & 0x80000000) !== 0;
  const flipV = (d & 0x40000000) !== 0;
  const flipD = (d & 0x20000000) !== 0;
  d &= 0x0fffffff;
  let source = null;
  for (let i = data.tilesets.length - 1; i >= 0; i--) {
    const ts = data.tilesets[i];
    if (d >= ts.firstgid) {
      source = ts.source;
      d -= ts.firstgid;
      break;
    }
  }
  return { flipH, flipV, flipD, source, d, x, y, layerData };
};

const validateTile = (tile) => {
  let err = null;
  if (tile.flipH || tile.flipV || tile.flipD) {
    err = 'Cannot flip tile';
  } else if (tile.source === null) {
    if (tile.d !== 0) {
      err = 'Unknown source';
    }
  } else if (tile.source === '../../design/tiles.5x5.tsx') {
    if (tile.d < 0 || tile.d >= 512) {
      err = `Tile data ${tile.d} out of range`;
    }
  } else {
    err = `Invalid source: ${tile.source}`;
  }
  if (err) {
    const layer = tile.layerData === background
      ? 'background'
      : tile.layerData === objects
      ? 'objects'
      : 'unknown';
    throw new Error(`Bad tile at ${layer}(${tile.x}, ${tile.y}): ${err}`);
  }
  return tile.d;
};

// maps tiles to logic type
// 0 nothing
// 1 solid
// 2 anchor
// 3 jetpack
// 4 balloon
// 5 fuse
// 6 tnt
// 7 non-solid wire
// 8 solid wire
// 9 coin
// 10 spike
// 11 ladder
const logicTable = [
  0, 0, 0, 0,10,10,11,11, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 6, 5, 2, 3, 4, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0,
];

for (let y = 0; y < data.height; y++) {
  for (let x = 0; x < data.width; x++) {
    let d1 = validateTile(tileAt(background, x, y));
    let d2 = validateTile(tileAt(objects, x, y));
    let logic = logicTable[d2];
    if (logic === 7) { // non-solid wire
      if (logicTable[d1]) {
        logic = 8; // solid wire
      }
    } else if (logic === 0) {
      logic = logicTable[d1];
    }
    if (d1 >= 64) d1 -= 64;
    if (d2 >= 64) d2 -= 64;
    out.push(d1 | (d2 << 6) | (logic << 12));
  }
}

// output regions
const regionTypes = ['inside', 'start'];
out.push(regions.length);
for (const rect of regions) {
  const t = regionTypes.indexOf(rect.type);
  if (t < 0) throw new Error(`Unknown region type: ${rect.type}`);
  out.push(t);
  out.push(Math.floor(rect.x / 10));
  out.push(Math.floor(rect.y / 10));
  out.push(Math.floor(rect.width / 10));
  out.push(Math.floor(rect.height / 10));
}

// output file
const a = new Uint16Array(out);
await fs.writeFile(outputFile, Buffer.from(a.buffer, a.byteOffset, a.byteLength));

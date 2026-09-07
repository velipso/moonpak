// SPDX-License-Identifier: 0BSD

console.log('// SPDX-License-Identifier: 0BSD\n// Generated via: node scripts/snddata.js');

const DUTY_COUNT = 8;

const bands = [
  { lowpass: 1, size: 1024 },
  { lowpass: 2, size:  512 },
  { lowpass: 3, size:  256 },
  { lowpass: 4, size:  128 },
  { lowpass: 5, size:  128 },
  { lowpass: 6, size:  128 },
  { lowpass: 7, size:  128 },
  { lowpass: 8, size:  128 },
  { lowpass: 9, size:  128 },
];

const sineSizes = [1024, 512, 256, 128];

function generateWave({
  band,
  duty = 0,
  square = 0,
  sine = 0,
  saw = 0,
  triangle = 0,
}) {
  const { lowpass, size } = bands[band];
  if (duty < 0.5) duty = 1 - duty;
  // v1: https://www.desmos.com/calculator/jztbyinfbz  (equal area square wave)
  // v2: https://www.desmos.com/calculator/adyrewgqco  (max amplitude square wave)

  // use the lowpass variable to cap the number of harmonics
  // this is the standard way to implement a lowpass filter with additive synthesis
  const maxHarmonic = 1024 >> lowpass;
  const out = new Int16Array(size);
  for (let i = 0; i < size; i++) {
    const phase = i * Math.PI * 2 / size;
    let value = 0;

    for (let n = 1; n < maxHarmonic; n++) {
      const nPi = n * Math.PI;
      const piFactor = 2 / nPi;

      const bSine = n === 1 ? 1 : 0;

      const aSquare = piFactor * Math.sin(duty * 2 * nPi);
      const sinTerm = Math.sin(duty * nPi);
      const bSquare = piFactor * 2 * sinTerm * sinTerm;

      const bSaw = piFactor * ((n & 1) ? 1 : -1);

      const bTriangle = (n & 1)
        ? 2 * piFactor * piFactor * ((((n - 1) >> 1) & 1) ? -1 : 1)
        : 0;

      const real = square * aSquare;
      const imag =
        square * bSquare +
        sine * bSine +
        saw * bSaw +
        triangle * bTriangle;

      value +=
        imag * Math.sin(phase * n) +
        real * Math.cos(phase * n);
    }

    value = Math.max(-1, Math.min(1, value));
    out[i] = Math.round(value * 16383);
  }

  return out;
}

function generateSine(size) {
  const out = new Int16Array(size);
  for (let i = 0; i < size; i++) {
    out[i] = Math.round(Math.sin(i * Math.PI * 2 / size) * 16383);
  }
  return out;
}

let offset = 0;
function emit(comment, data) {
  console.log(`// ${comment} (offset ${offset}, count ${data.length})`);
  offset += data.length;
  const out = [];
  for (let i = 0; i < data.length; i++) {
    if ((i % 8) === 0) out.push('');
    if (out[out.length - 1]) out[out.length - 1] += ' ';
    let v = data[i];
    if (v < 0) v += 0x10000;
    out[out.length - 1] += `0x${`000${v.toString(16)}`.substr(-4)},`;
  }
  console.log(out.join('\n'));
}

// channel kind = 0 (sine)
for (const size of sineSizes) {
  emit(`sine ${size}`, generateSine(size));
}

// channel kind = 1 (triangle)
for (let band = 0; band < bands.length; band++) {
  emit(`triangle LP${bands[band].lowpass}`, generateWave({ band, triangle: 1 }));
}

// channel kind = 2 (saw)
for (let band = 0; band < bands.length; band++) {
  emit(`saw LP${bands[band].lowpass}`, generateWave({ band, saw: 1 }));
}

// channel kind = 3 (square) -- for each duty
for (let d = 0; d < DUTY_COUNT; d++) {
  const duty = (d + 1) / (2 * DUTY_COUNT);
  for (let band = 0; band < bands.length; band++) {
    emit(`square${d + 1} LP${bands[band].lowpass}`, generateWave({ band, duty, square: 1 }));
  }
}

console.log(`// total count ${offset}`);

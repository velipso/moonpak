// SPDX-License-Identifier: 0BSD
// sample rate is 32768 samples/sec, or 1 sample every 512 cycles, so target samples per frame:
// 280896 cycles per frame / 512 cycles per sample = 548.625 samples per frame
// this is spread over 32 frames, most frames having 548 samples, but some having 552 samples:
548, 548, 548, 548, 548, 548, 552,
548, 548, 548, 548, 548, 552,
548, 548, 548, 548, 548, 548, 552,
548, 548, 548, 548, 548, 552,
548, 548, 548, 548, 548, 552,

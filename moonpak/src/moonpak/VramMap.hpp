// SPDX-License-Identifier: 0BSD
//
// Utility functions for managing text and affine maps.
//
// Start by defining a copyRegion function:
//
// void copyTextRegion(
//   int dstOffset, // destination offset
//   int dstStride, // destination cells per row
//   int sx,        // source x (measured in cells)
//   int sy,        // source y
//   int w,         // width
//   int h,         // height
//   void *user     // user provided value to .copy(...)
// ) {
//   // example:
//   uint16_t *dst = VramMap::fromMapBase<uint16_t>(18) + offset; // mapBase (18) comes from BGXCNT
//   for (int y = 0; y < h; y++) {
//     for (int x = 0; x < w; x++) {
//       dst[x] = worldData[(sx + x) + (sy + y) * worldWidth];
//     }
//     // go to next row, so increment dst by dstStride
//     dst += dstStride;
//   }
// }
//
// Then, hook up a VramMap (for example, 512x512 pixel map):
//
// using TextMap = VramMap::Text<512, 512>;
//
// Now, you can copy a region of the map:
//
// TextMap::copy(0, 0, 32, 32, copyTextRegion, user);
//
// For affine backgrounds, it's mostly the same, except the copyRegion receives a `uint8_t *`:
//
// void copyAffineRegion(
//   int dstOffset, // destination offset
//   int dstStride, // destination cells per row
//   int sx,        // source x (measured in cells)
//   int sy,        // source y
//   int w,         // width
//   int h,         // height
//   void *user     // user provided value to .copy(...)
// ) {
//   uint8_t *dst = VramMap::fromMapBase<uint8_t>(20) + offset; // mapBase (20) comes from BGXCNT
//   // WARNING:
//   // This function will receive odd-aligned pointers!
//   // The GBA does not support 8-bit writes to VRAM, so you must either handle
//   // packing writes into 16 or 32-bits (perhaps using read-modify-write), or
//   // use a buffer in EWRAM and copy it to VRAM every vblank (for example).
// }
//
// using AffineMap = VramMap::Affine<512, 512>;
//
// AffineMap::copy(0, 0, 32, 32, copyAffineRegion, user);
//
#pragma once
#include <stdint.h>

namespace moonpak {

template<typename Fn>
concept VramMapCopyRegion = requires(
  Fn fn,
  int dstOffset,
  int dstStride,
  int sx,
  int sy,
  int w,
  int h,
  void *user
) {
  fn(dstOffset, dstStride, sx, sy, w, h, user);
};

namespace VramMap {
  template <typename Cell>
  static inline Cell *fromMapBase(int mapBase) {
    return (Cell *)(0x06000000 + mapBase * 0x800);
  }

  template <int Width, int Height>
  struct Text {
    static_assert(Width == 256 || Width == 512);
    static_assert(Height == 256 || Height == 512);
    static constexpr int PlaneSize = 32; // 32 tiles
    static constexpr int Shift = 5; // 2^Shift == PlaneSize
    static constexpr int Mask = (1 << Shift) - 1;
    static constexpr int Stride = PlaneSize;
    static constexpr int PlaneArea = PlaneSize * PlaneSize;

    template<VramMapCopyRegion Fn>
    static void copy(int sx, int sy, int w, int h, Fn copyRegion, void *user = nullptr) {
      if (w <= 0 || h <= 0) return;

      int px1 = sx >> Shift;
      int py1 = sy >> Shift;
      int px2 = (sx + w - 1) >> Shift;
      int py2 = (sy + h - 1) >> Shift;

      int oy = sy & Mask;
      int leftY = h;
      for (int py = py1; py <= py2; py++) {
        int lh = PlaneSize - oy;
        if (lh > leftY) lh = leftY;
        int ox = sx & Mask;
        int leftX = w;
        int sx_ = sx;
        for (int px = px1; px <= px2; px++) {
          int lw = PlaneSize - ox;
          if (lw > leftX) lw = leftX;

          int offset = ox + oy * Stride;
          if constexpr (Width == 256) {
            if constexpr (Height == 256) {
              // do nothing
            } else { // Height == 512
              offset += (py & 1) * PlaneArea;
            }
          } else { // Width == 512
            if constexpr (Height == 256) {
              offset += (px & 1) * PlaneArea;
            } else { // Height == 512
              offset += (px & 1) * PlaneArea + (py & 1) * PlaneArea * 2;
            }
          }
          copyRegion(offset, Stride, sx_, sy, lw, lh, user);

          ox = 0;
          sx_ += lw;
          leftX -= lw;
        }
        oy = 0;
        sy += lh;
        leftY -= lh;
      }
    }
  };

  template <int Width, int Height>
  struct Affine {
    static_assert(Width == 128 || Width == 256 || Width == 512 || Width == 1024);
    static_assert(Width == Height);
    static constexpr int MapWidth = Width / 8;
    static constexpr int MapHeight = Height / 8;
    static constexpr int Shift = Width == 128 ? 4 : Width == 256 ? 5 : Width == 512 ? 6 : 7;
    static constexpr int Mask = (1 << Shift) - 1;
    static constexpr int Stride = MapWidth;

    template<VramMapCopyRegion Fn>
    static void copy(int sx, int sy, int w, int h, Fn copyRegion, void *user = nullptr) {
      if (w <= 0 || h <= 0) return;

      int px1 = sx >> Shift;
      int py1 = sy >> Shift;
      int px2 = (sx + w - 1) >> Shift;
      int py2 = (sy + h - 1) >> Shift;

      int oy = sy & Mask;
      int leftY = h;
      for (int py = py1; py <= py2; py++) {
        int lh = MapHeight - oy;
        if (lh > leftY) lh = leftY;
        int ox = sx & Mask;
        int leftX = w;
        int sx_ = sx;
        for (int px = px1; px <= px2; px++) {
          int lw = MapWidth - ox;
          if (lw > leftX) lw = leftX;

          copyRegion(ox + oy * Stride, Stride, sx_, sy, lw, lh, user);

          ox = 0;
          sx_ += lw;
          leftX -= lw;
        }
        oy = 0;
        sy += lh;
        leftY -= lh;
      }
    }
  };
};

} // moonpak

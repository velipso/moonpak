// SPDX-License-Identifier: 0BSD
#include "main.hpp"
#include "types/Player.hpp"
#include "data/palette.hpp"
#include "data/spritesheets/digits.8x8.hpp"
#include "data/animations.hpp"
#include "data/songs/outro.hpp"
#include "data/tilemaps/world.hpp"
#include "data/tilesheets/tiles.5x5.hpp"

Oam g_oam;
VramObj g_vramObj;
Spr g_spr;
Snd g_snd(2, 4);

static uint8_t g_bg2MapBuffer[64 * 64];
static uint8_t g_bg3MapBuffer[64 * 64];

using TileMap = VramMap::Affine<512, 512>;

static struct {
  int x; // upper-left world pixel coordinate
  int y;
  int mapX; // upper-left tile loaded onto screen
  int mapY;
} g_camera;

static const int scroll[] = {
  // for (var i = 0; i < 320; i++) console.log(Math.round(i * 256 * 8/5));
  0, 410, 819, 1229, 1638, 2048, 2458, 2867, 3277, 3686, 4096, 4506, 4915, 5325, 5734, 6144, 6554,
  6963, 7373, 7782, 8192, 8602, 9011, 9421, 9830, 10240, 10650, 11059, 11469, 11878, 12288, 12698,
  13107, 13517, 13926, 14336, 14746, 15155, 15565, 15974, 16384, 16794, 17203, 17613, 18022, 18432,
  18842, 19251, 19661, 20070, 20480, 20890, 21299, 21709, 22118, 22528, 22938, 23347, 23757, 24166,
  24576, 24986, 25395, 25805, 26214, 26624, 27034, 27443, 27853, 28262, 28672, 29082, 29491, 29901,
  30310, 30720, 31130, 31539, 31949, 32358, 32768, 33178, 33587, 33997, 34406, 34816, 35226, 35635,
  36045, 36454, 36864, 37274, 37683, 38093, 38502, 38912, 39322, 39731, 40141, 40550, 40960, 41370,
  41779, 42189, 42598, 43008, 43418, 43827, 44237, 44646, 45056, 45466, 45875, 46285, 46694, 47104,
  47514, 47923, 48333, 48742, 49152, 49562, 49971, 50381, 50790, 51200, 51610, 52019, 52429, 52838,
  53248, 53658, 54067, 54477, 54886, 55296, 55706, 56115, 56525, 56934, 57344, 57754, 58163, 58573,
  58982, 59392, 59802, 60211, 60621, 61030, 61440, 61850, 62259, 62669, 63078, 63488, 63898, 64307,
  64717, 65126, 65536, 65946, 66355, 66765, 67174, 67584, 67994, 68403, 68813, 69222, 69632, 70042,
  70451, 70861, 71270, 71680, 72090, 72499, 72909, 73318, 73728, 74138, 74547, 74957, 75366, 75776,
  76186, 76595, 77005, 77414, 77824, 78234, 78643, 79053, 79462, 79872, 80282, 80691, 81101, 81510,
  81920, 82330, 82739, 83149, 83558, 83968, 84378, 84787, 85197, 85606, 86016, 86426, 86835, 87245,
  87654, 88064, 88474, 88883, 89293, 89702, 90112, 90522, 90931, 91341, 91750, 92160, 92570, 92979,
  93389, 93798, 94208, 94618, 95027, 95437, 95846, 96256, 96666, 97075, 97485, 97894, 98304, 98714,
  99123, 99533, 99942, 100352, 100762, 101171, 101581, 101990, 102400, 102810, 103219, 103629,
  104038, 104448, 104858, 105267, 105677, 106086, 106496, 106906, 107315, 107725, 108134, 108544,
  108954, 109363, 109773, 110182, 110592, 111002, 111411, 111821, 112230, 112640, 113050, 113459,
  113869, 114278, 114688, 115098, 115507, 115917, 116326, 116736, 117146, 117555, 117965, 118374,
  118784, 119194, 119603, 120013, 120422, 120832, 121242, 121651, 122061, 122470, 122880, 123290,
  123699, 124109, 124518, 124928, 125338, 125747, 126157, 126566, 126976, 127386, 127795, 128205,
  128614, 129024, 129434, 129843, 130253, 130662
};

static inline int div5(int v) {
  return v < 0 ? -((-v * 3277) >> 14) : (v * 3277) >> 14;
}

static inline int div10(int v) {
  return v < 0 ? -((-v * 3277) >> 15) : (v * 3277) >> 15;
}

static inline int div320(int v) {
  return v < 0 ? -((-v * 3277) >> 20) : (v * 3277) >> 20;
}

static inline int clamp(int v, int min, int max) {
  return v < min ? min : v > max ? max : v;
}

static void irq_vblank() {
  Inp::update();
  Rnd::update();
  int mx = g_camera.x - div320(g_camera.x) * 320;
  int my = g_camera.y - div320(g_camera.y) * 320;
  int x = scroll[mx];
  int y = scroll[my];
  Reg::BG2X::set(x);
  Reg::BG2Y::set(y);
  Reg::BG3X::set(x);
  Reg::BG3Y::set(y);
  g_oam.copy();
  g_spr.copy();
  memcpy32(VramMap::fromMapBase<void *>(16), g_bg2MapBuffer, 64 * 64);
  memcpy32(VramMap::fromMapBase<void *>(20), g_bg3MapBuffer, 64 * 64);
}

static inline void nextframe() {
  g_spr.tick();
  Swi::vblankIntrWait();
  g_snd.copy();
}

static void copyTileMap(
  int dstOffset, // destination offset
  int dstStride, // destination cells per row
  int sx,        // source x (measured in cells)
  int sy,        // source y
  int w,         // width
  int h,         // height
  void *user     // user provided value to .copy(...)
) {
  uint8_t *map1 = g_bg3MapBuffer + dstOffset;
  uint8_t *map2 = g_bg2MapBuffer + dstOffset;

  const uint16_t *mapData = (const uint16_t *)dataTilemapsWorld;
  int mapWidth = mapData[0];
  mapData += 2;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int tx = x + sx;
      int ty = y + sy;

      int tile2 = mapData[(tx >> 1) + (ty >> 1) * mapWidth];
      int tile1 = tile2 & 0x3f;
      tile2 = (tile2 >> 8) & 0x3f;

      int tile1X = tile1 & 0xf;
      int tile1Y = tile1 >> 4;
      tile1 = tile1Y * 64 + tile1X * 2;

      int tile2X = tile2 & 0xf;
      int tile2Y = tile2 >> 4;
      tile2 = tile2Y * 64 + tile2X * 2;

      if (!(ty & 1)) {
        if (!(tx & 1)) {
          map1[x] = tile1;
          map2[x] = tile2;
        } else {
          map1[x] = tile1 + 1;
          map2[x] = tile2 + 1;
        }
      } else {
        if (!(tx & 1)) {
          map1[x] = tile1 + 32;
          map2[x] = tile2 + 32;
        } else {
          map1[x] = tile1 + 33;
          map2[x] = tile2 + 33;
        }
      }
    }
    map1 += dstStride;
    map2 += dstStride;
  }
}

static void camera_set(int x, int y) {
  const uint16_t *mapData = (const uint16_t *)dataTilemapsWorld;
  int mapWidth = *mapData++;
  int mapHeight = *mapData++;

  x = clamp(x, 0, mapWidth * 10 - 240);
  y = clamp(y, 0, mapHeight * 10 - 160);

  g_camera.x = x;
  g_camera.y = y;

  int mapX = div5(x);
  int mapY = div5(y);

  int dx = clamp(mapX - g_camera.mapX, -64, 64);
  int dy = clamp(mapY - g_camera.mapY, -64, 64);

  g_camera.mapX = mapX;
  g_camera.mapY = mapY;

  if (dx < 0) {
    TileMap::copy(g_camera.mapX, g_camera.mapY, -dx, 64, copyTileMap);
  } else if (dx > 0) {
    TileMap::copy(g_camera.mapX + 64 - dx, g_camera.mapY, dx, 64, copyTileMap);
  }
  if (dy < 0) {
    TileMap::copy(g_camera.mapX, g_camera.mapY, 64, -dy, copyTileMap);
  } else if (dy > 0) {
    TileMap::copy(g_camera.mapX, g_camera.mapY + 64 - dy, 64, dy, copyTileMap);
  }
}

extern "C" int main() {
  Irq::init();
  Irq::vblank(irq_vblank);

  Reg::DISPCNT::write()
    .mode(2)
    .bg2(1)
    .bg3(1)
    .obj(1)
    .done();
  Reg::DISPSTAT::update()
    .vblank(1)
    .done();
  Reg::IE::update()
    .vblank(1)
    .done();
  Reg::IME::set(1);

  memcpy32((void *)0x05000000, dataPalette, dataPaletteSize);
  memcpy32((void *)0x05000200, dataPalette, dataPaletteSize);

  g_snd.loadSong(0, dataSongsOutro, 0);

  Reg::BG2X::set(0);
  Reg::BG2Y::set(0);
  Reg::BG2PA::set(0x019a); // 8/5 * 0x100
  Reg::BG2PB::set(0);
  Reg::BG2PC::set(0);
  Reg::BG2PD::set(0x019a); // 8/5 * 0x100
  Reg::BG3X::set(0);
  Reg::BG3Y::set(0);
  Reg::BG3PA::set(0x019a); // 8/5 * 0x100
  Reg::BG3PB::set(0);
  Reg::BG3PC::set(0);
  Reg::BG3PD::set(0x019a); // 8/5 * 0x100

  Reg::BG2CNT::write()
    .priority(0)
    .tileBase(1)
    .is256(1)
    .mapBase(16)
    .screenSize(2)
    .wrap(1)
    .done();
  Reg::BG3CNT::write()
    .priority(0)
    .tileBase(0)
    .is256(1)
    .mapBase(20)
    .screenSize(2)
    .wrap(1)
    .done();

  // load tilesheet into memory, expanding 5x5 -> 8x8
  const uint8_t expand[] = {
     0,  1,  2,  2,  3,  3,  4,  4,
     5,  6,  7,  7,  8,  8,  9,  9,
    10, 11, 12, 12, 13, 13, 14, 14,
    10, 11, 12, 12, 13, 13, 14, 14,
    15, 16, 17, 17, 18, 18, 19, 19,
    15, 16, 17, 17, 18, 18, 19, 19,
    20, 21, 22, 22, 23, 23, 24, 24,
    20, 21, 22, 22, 23, 23, 24, 24,
  };
  const uint8_t *src = dataTilesheetsTiles5x5;
  volatile uint8_t *dst = (volatile uint8_t *)0x06000000;
  for (int ty = 0; ty < 32; ty++) {
    for (int tx = 0; tx < 16; tx++) {
      uint8_t data[8 * 8] = {0};
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          data[x + y * 8] = src[expand[x + y * 8]];
        }
      }
      memcpy64bytes((void *)dst, data);
      src += 25;
      dst += 64;
    }
  }

  // initialize camera off-map, so camera_set will paint entire map
  const uint16_t *mapData = (const uint16_t *)dataTilemapsWorld;
  int mapWidth = mapData[0];
  int mapHeight = mapData[1];
  g_camera.x = mapWidth * 10;
  g_camera.y = mapHeight * 10;
  g_camera.mapX = mapWidth;
  g_camera.mapY = mapHeight;

  camera_set(0, 0);

  int x = g_camera.x;
  int y = g_camera.y;
  int step = 7;
  for (;;) {
    if (Inp::Select.set()) {
      if (Inp::Up.hit() && y > 0) y--;
      if (Inp::Right.hit()) x++;
      if (Inp::Down.hit()) y++;
      if (Inp::Left.hit() && x > 0) x--;
    } else {
      if (Inp::Up.set() && y > step - 1) y -= step;
      if (Inp::Right.set()) x += step;
      if (Inp::Down.set()) y += step;
      if (Inp::Left.set() && x > step - 1) x -= step;
    }
    if (Inp::Start.hit()) {
      log("%x %x", x, y);
    }
    if (x != g_camera.x || y != g_camera.y) {
      camera_set(x, y);
    }
    // game logic
    nextframe();
  }
  return 0;
}

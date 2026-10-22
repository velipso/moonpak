// SPDX-License-Identifier: 0BSD
#include "main.hpp"
#include "types/MapCell.hpp"
#include "types/MapLogic.hpp"
#include "data/palette.hpp"
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
static MapLogic *g_logic;
static int g_mapWidth;
static int g_mapHeight;
static int g_homeX;
static int g_homeY;
static int g_pushed;
static inline constexpr Fix Q{8};
static int s_cursor;
static int s_place;

using TileMap = VramMap::Affine<512, 512>;

static struct {
  int x; // upper-left world pixel coordinate
  int y;
  int mapX; // upper-left tile loaded onto screen
  int mapY;
  int followX;
  int followY;
  bool followSnap;
} g_camera;

static struct {
  int sprite;
  int x;
  int y;
  int fx; // force
  int fy;
  int vx; // velocity
  int vy;
  int dead;
  int gx; // grounded
  int gy;
  bool grounded;
} g_user;

static struct {
  int sprite;
  int item;
  int dir;
  int grabbing;
  int placing;
  int tx; // source tile
  int ty;
} g_grab;

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

static void irq_vblank() {
  Inp::update();
  Reg::IME::set(1); // allow sound engine to interrupt vblank
  Rnd::update();

  int mx = Util::emod<320>(Q.floor(g_camera.x));
  int my = Util::emod<320>(Q.floor(g_camera.y));
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
  g_spr.world(Q.floor(g_camera.x), Q.floor(g_camera.y));
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

  const uint16_t *mapData = ((const uint16_t *)dataTilemapsWorld) + 2;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int tx = x + sx;
      int ty = y + sy;

      int k = (tx >> 1) + (ty >> 1) * g_mapWidth;
      MapCell cell = { mapData[k] };
      int tile1 = cell.bg1();
      int tile2 = cell.bg2();

      if (g_logic[k].dirty()) {
        switch (g_logic[k].type()) {
          case 0: // nothing
            tile2 = 0;
            break;
          case 2: // anchor
            tile2 = 51;
            break;
          case 3: // jetpack
            tile2 = 52;
            break;
          case 4: // balloon
            tile2 = 53;
            break;
        }
      }

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
  x = Util::clamp(x, 0, Q(g_mapWidth * 10 - 240));
  y = Util::clamp(y, 0, Q(g_mapHeight * 10 - 160));

  if (x == g_camera.x && y == g_camera.y) return;

  g_camera.x = x;
  g_camera.y = y;

  int mapX = Util::div<5>(Q.floor(x));
  int mapY = Util::div<5>(Q.floor(y));

  TileMap::scroll(g_camera.mapX, g_camera.mapY, mapX, mapY, copyTileMap);

  g_camera.mapX = mapX;
  g_camera.mapY = mapY;
}

static MapLogicFlags map_logic_tile(int tx, int ty, MapLogicFlags result) {
  if (tx < 0 || tx >= g_mapWidth || ty < 0 || ty >= g_mapHeight) {
    return result.flags(0).solid(1).spike(1);
  }

  if (tx == g_homeX && ty == g_homeY) {
    result.home(1);
  }

  MapLogic logic = g_logic[tx + ty * g_mapWidth];
  result.inside(result.inside() | logic.inside());
  if (logic.type()) {
    result.type(logic.type());
  }

  switch (logic.type()) {
    case 0: // nothing
      break;
    case 1: // solid
      result.solid(1);
      break;
    case 2: // anchor
      result.solid(1).grab(1);
      break;
    case 3: // jetpack
      result.solid(1).grab(1);
      break;
    case 4: // balloon
      result.solid(1).grab(1);
      break;
    case 5: // fuse
      result.solid(1).grab(1);
      break;
    case 6: // tnt
      result.solid(1).explode(1);
      break;
    case 7: // non-solid wire
      result.explode(1);
      break;
    case 8: // solid wire
      result.solid(1).explode(1);
      break;
    case 9: // coin
      result.coin(1);
      break;
    case 10: // spike
      result.solid(1).spike(1);
      break;
    case 11: // ladder
      result.ladder(1);
      break;
  }
  return result;
}

static MapLogicFlags map_logic_tile(int tx, int ty) {
  MapLogicFlags result = {0};
  return map_logic_tile(tx, ty, result);
}

static MapLogicFlags map_logic(int x, int y, MapLogicFlags result) {
  int tx = Util::div<10>(Q.floor(x));
  int ty = Util::div<10>(Q.floor(y));
  return map_logic_tile(tx, ty, result);
}

static MapLogicFlags map_logic(int x, int y) {
  MapLogicFlags result = {0};
  return map_logic(x, y, result);
}

static void map_logic_set_type(int tx, int ty, int type) {
  g_logic[tx + ty * g_mapWidth].type(type).dirty(1);
  TileMap::copy(tx << 1, ty << 1, 2, 2, copyTileMap);
}

extern "C" int main() {
  Irq::init();
  Irq::vblank(irq_vblank);

  Reg::DISPCNT::write()
    .obj1D(1)
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

  // load map size
  const uint16_t *mapData = (const uint16_t *)dataTilemapsWorld;
  g_mapWidth = *mapData++;
  g_mapHeight = *mapData++;

  // load logic
  g_logic = (MapLogic *)calloc(g_mapWidth * g_mapHeight, sizeof(MapLogic));
  for (int y = 0, k = 0; y < g_mapHeight; y++) {
    for (int x = 0; x < g_mapWidth; x++, k++) {
      MapCell cell = { *mapData++ };
      g_logic[k].type(cell.logic());
    }
  }

  // load and process regions
  int totalRegions = *mapData++;
  for (int r = 0; r < totalRegions; r++) {
    int type = *mapData++;
    int x = *mapData++;
    int y = *mapData++;
    int width = *mapData++;
    int height = *mapData++;
    switch (type) {
      case 0: // inside
        for (int dy = 0; dy < height; dy++) {
          for (int dx = 0; dx < width; dx++) {
            g_logic[x + dx + (y + dy) * g_mapWidth].inside(1);
          }
        }
        break;
      case 1: // start
        g_homeX = g_user.x = Q(x * 10);
        g_homeY = g_user.y = Q(y * 10);
        break;
    }
  }

  // initialize camera off-map, so camera_set will paint entire map
  g_camera.x = g_mapWidth * 10;
  g_camera.y = g_mapHeight * 10;
  g_camera.mapX = g_mapWidth;
  g_camera.mapY = g_mapHeight;
  g_camera.followSnap = true;
  camera_set(0, 0);

  g_user.sprite = g_spr.alloc(16, 16, 1, true);
  g_spr.entry(g_user.sprite)
    .show(true)
    .anim(Anim::heroC);

  s_cursor = g_spr.alloc(16, 16, 0, true);
  g_spr.entry(s_cursor)
    .show(true)
    .anim(Anim::cursor);

  s_place = g_spr.alloc(16, 16, 2, true);
  g_spr.entry(s_place)
    .show(true)
    .anim(Anim::place);

  g_grab.item = 0;
  g_grab.grabbing = 0;
  g_grab.placing = 0;
  g_grab.sprite = g_spr.alloc(16, 16, 1, true);

  for (;;) {
    { // move user
      g_user.fx = 0;
      g_user.fy = 0;
      int h = 0;
      int v = 0;

      /*
      if (!grabItem && input.hitGrab && grounded && pushed && !overlaps(userX, userY - 9).collide) {
        const ax = Math.floor(userX / 9) + pushed;
        const { item } = getForce(ax, Math.floor(userY / 9));
        if (item > 0) {
          if (tutorial === 4 || tutorial === 5)
            tutorial = 6;
          sfx.lift.play();
          const k = ax + cy * worldWidth;
          world[k] = world[k] & maskNoItem;
          // check for falling items
          itemFalling = [];
          for (let ay = cy - 1; ay >= 0; ay--) {
            const { item } = getForce(ax, ay);
            if (!item)
              break;
            const k = ax + ay * worldWidth;
            world[k] = world[k] & maskNoItem;
            itemFalling.push([ax, ay, item]);
          }
          ...
          if ((grabItem & 0xfff) === 4) {
            const k = ax + (cy + 1) * worldWidth;
            world[k] = (world[k] & maskNoItem) | (6 << 15);
            explode(ax, cy + 1, 30, 5);
            exploded.push(k - worldWidth); // remove fuse
          }
        }
      }
      */
      if (
        !g_grab.item && // no currently grabbed item
        Inp::B.hit() && // are we trying to grab?
        g_user.grounded && // are we on the ground?
        g_pushed && // are we pushing against something?
        !map_logic(g_user.x, g_user.y - Q(10)).solid() // is there room above us to lift something?
      ) {
        int tx = Util::div<10>(Q.floor(g_user.x));
        int ty = Util::div<10>(Q.floor(g_user.y));
        MapLogicFlags flags = map_logic_tile(tx + g_pushed, ty);
        if (flags.grab()) {
          // grab the item!
          g_user.fx = 0;
          g_user.fy = 0;
          g_user.vx = 0;
          g_user.vy = 0;
          g_grab.item = flags.type();
          g_grab.dir = g_pushed;
          g_grab.grabbing = 1;
          g_grab.tx = tx + g_pushed;
          g_grab.ty = ty;
          g_spr.show(g_grab.sprite, true).priority(g_grab.sprite, 0);
          switch (flags.type()) {
            case 2: g_spr.anim(g_grab.sprite, Anim::itemAnchor); break;
            case 3: g_spr.anim(g_grab.sprite, Anim::itemJetpack); break;
            case 4: g_spr.anim(g_grab.sprite, Anim::itemBalloon); break;
            case 5: g_spr.anim(g_grab.sprite, Anim::itemFuse); break;
          }
          // move user to edge of pixel boundary
          g_user.x = g_pushed > 0 ? Q(tx * 10 + 1) - 1 : Q(tx * 10);
          g_user.y = Q(ty * 10 + 1) - 1;
          map_logic_set_type(tx + g_pushed, ty, 0);
          g_pushed = 0;
        }
      } else if (g_grab.item && Inp::B.hit() && g_user.grounded) {
        // TODO: placing item
        /*
        placePos = [groundedPos[0], groundedPos[1] - 1];
        let canPlace = true;
        if (isPrize(grabItem)) {
          // verify we can place it here, at home
          const p = prizes.find(p => p.item === grabItem);
          canPlace = placePos[0] === p.hx && placePos[1] === p.hy;
        } else {
          // don't allow other objects in prize homes
          canPlace = !prizes.find(p => placePos[0] === p.hx && placePos[1] === p.hy);
        }
        if (canPlace) {
          sfx.drop.play();
          forceX = 0;
          forceY = 0;
          velX = 0;
          velY = 0;
          placing = 1;
          placeUserMove = [userY, userY - 9];
        }
        */
        g_user.fx = 0;
        g_user.fy = 0;
        g_user.vx = 0;
        g_user.vy = 0;
        g_grab.placing = 1;
        g_spr.priority(g_grab.sprite, 0);
      } else if (g_grab.grabbing || g_grab.placing) {
        // don't allow movement
      } else {
        h = Inp::dx();
        if (Inp::A.hit() && g_user.grounded) {
          g_user.fy += Q(-6.67);
        }
      }

      g_user.fx += h * Q(0.5);
      int gravity = Q(0.2);
      g_user.vx += g_user.fx;
      g_user.vy += gravity + g_user.fy;
      g_user.vx = Q.mul(g_user.vx, g_user.grounded ? Q(0.8) : Q(0.85));
      g_user.vy = Q.mul(g_user.vy, Q(0.98));

      // move user
      int remainderX = Q.frac(g_user.x);
      int currentSX = g_user.vx;
      int remainderY = Q.frac(g_user.y);
      int currentSY = g_user.vy;
      int ox, oy, osx, osy;
      bool doneX = false;
      bool doneY = false;
      int change = 0;
      int ioy = g_grab.item ? Q(-10) : 0;
      g_user.grounded = false;

      while ((currentSX != 0 && !doneX) || (currentSY != 0 && !doneY)) {
        ox = g_user.x;
        oy = g_user.y;
        osx = currentSX;
        osy = currentSY;

        { // step X
          if (currentSX > 0) {
            // enough velocity to advance one pixel?
            if (currentSX + remainderX >= Q(1)) {
              // advance one pixel
              g_user.x += Q(1) - remainderX;
              currentSX -= Q(1) - remainderX;
              remainderX = 0;
            } else {
              // not enough velocity to advance one pixel, so just consume rest
              g_user.x += currentSX;
              currentSX = 0;
            }
            change = 1;
          } else if (currentSX < 0) {
            // any remainder?
            if (remainderX + currentSX < 0 && remainderX != 0) {
              // transfer remainder
              currentSX += remainderX;
              g_user.x -= remainderX;
              remainderX = Q(1); // go left next iteration
            } else {
              // not enough velocity to advance one pixel, so just consume rest
              g_user.x += currentSX;
              currentSX = 0;
            }
            change = -1;
          } else {
            change = 0;
          }
        }

        // check for collision
        if (change) {
          int ux = g_user.x + (change > 0 ? Q(9) : 0);
          MapLogicFlags flags = map_logic(
            ux,
            g_user.y + ioy,
            map_logic(ux, g_user.y, map_logic(ux, g_user.y + Q(9)))
          );
          if (flags.solid()) {
            g_pushed = flags.grab() ? change : 0;
            g_user.x = ox;
            g_user.vx = 0;
            currentSX = osx;
            doneX = true;
          } else {
            g_pushed = 0;
          }
        }

        { // step Y
          if (currentSY > 0) {
            // enough velocity to advance one pixel?
            if (currentSY + remainderY >= Q(1)) {
              // advance one pixel
              g_user.y += Q(1) - remainderY;
              currentSY -= Q(1) - remainderY;
              remainderY = 0;
            } else {
              // not enough velocity to advance one pixel, so just consume rest
              g_user.y += currentSY;
              currentSY = 0;
            }
            change = 1;
          } else if (currentSY < 0) {
            // any remainder?
            if (remainderY + currentSY < 0 && remainderY != 0) {
              // transfer remainder
              currentSY += remainderY;
              g_user.y -= remainderY;
              remainderY = Q(1); // go left next iteration
            } else {
              // not enough velocity to advance one pixel, so just consume rest
              g_user.y += currentSY;
              currentSY = 0;
            }
            change = -1;
          } else {
            change = 0;
          }
        }

        // check for collision
        if (change > 0) {
          int ty = Util::div<10>(Q.floor(g_user.y + Q(9)));
          int tx1 = Util::div<10>(Q.floor(g_user.x));
          int tx2 = Util::div<10>(Q.floor(g_user.x + Q(4.5)));
          int tx3 = Util::div<10>(Q.floor(g_user.x + Q(9)));
          MapLogicFlags f1 = map_logic_tile(tx1, ty);
          MapLogicFlags f2 = map_logic_tile(tx2, ty);
          MapLogicFlags f3 = map_logic_tile(tx3, ty);
          if (f2.solid()) {
            g_user.gx = tx2;
            goto y_solid_down;
          } else if (f1.solid()) {
            g_user.gx = tx1;
            goto y_solid_down;
          } else if (f3.solid()) {
            g_user.gx = tx3;
y_solid_down:
            g_user.gy = ty;
            g_user.grounded = true;
            goto y_solid;
          }
        } else if (change < 0) {
          if (map_logic(
            g_user.x,
            g_user.y + ioy,
            map_logic(g_user.x + Q(9), g_user.y + ioy)
          ).solid()) {
y_solid:
            g_user.y = oy;
            g_user.vy = 0;
            currentSY = osy;
            doneY = true;
          }
        }
      }

    /*
    function stepX() {
      // check for collision
      const { collide: userCollide, spike: userSpike, home: userHome } =
        overlaps(userX, userY);
      const { collide: itemCollide, spike: itemSpike } = overlaps(userX, userY - 9);
      if (grabItem && itemSpike)
        explodeItem();
      if (userSpike)
        explodeUser();
      if (userHome)
        checkBeatGame();
      if (userCollide || (grabItem && itemCollide)) {
        // revert
        userX = ox;
        velX = 0;
        currentSX = osx;
        pushed = currentSX < 0 ? -1 : currentSX > 0 ? 1 : false;
        doneX = true;
      } else if (ox !== userX) {
        pushed = false;
      }
    }

    function stepY() {
      // check for collision
      let userCollide, userSpike, userHome, itemCollide, itemSpike;
      const checkCollision = () => {
        const { collide: ucollide, spike: uspike, home: uhome } = overlaps(userX, userY);
        const { collide: icollide, spike: ispike } = overlaps(userX, userY - 9);
        userCollide = ucollide;
        userSpike = uspike;
        userHome = uhome;
        itemCollide = icollide;
        itemSpike = ispike;
        return ucollide || (grabItem && icollide);
      }
      let collision = checkCollision();
      if (collision && osy < -0.5) {
        // if we collided, then check if we are jumping up, and *near* a gap
        const checkAgain = (ux) => {
          if (Math.abs(ux) - userX > 2)
            return false;
          const old = userX;
          userX = ux;
          collision = checkCollision();
          if (collision) {
            userX = old;
            return false;
          }
          return true;
        };
        checkAgain(Math.floor(userX)) ||
        checkAgain(Math.floor(userX) - 1) ||
        checkAgain(Math.floor(userX) + 1) ||
        checkAgain(Math.floor(userX) - 2) ||
        checkAgain(Math.floor(userX) + 2);
      }

      if (grabItem && itemSpike)
        explodeItem();
      if (userSpike)
        explodeUser();
      if (userHome)
        checkBeatGame();
      if (collision) {
        // revert
        userY = oy;
        velY = 0;
        currentSY = osy;
        if (currentSY > 0) {
          const tx = (userX + 5) / 9;
          const ty = (userY + 5) / 9;
          const ncx = Math.floor(tx);
          const ncy = Math.floor(ty);
          const { solid } = getForce(ncx, ncy + 1);
          if (solid) {
            grounded = true;
            groundedPos = [ncx, ncy + 1];
          } else {
            grounded = false;
            const cx = Math.floor(userX / 9);
            const cy = Math.floor((userY + 1) / 9);
            const cw = Math.ceil((userX + 9) / 9);
            const ch = Math.ceil((userY + 1 + 9) / 9);
            for (let iy = cy; iy < ch && !grounded; iy++) {
              for (let ix = cx; ix < cw && !grounded; ix++) {
                if (getForce(ix, iy).solid) {
                  grounded = true;
                  groundedPos = [ix, iy];
                }
              }
            }
          }
        }
        doneY = true;
      }
    }
*/
    }

    { // stop user if moving really slowly
      int epsilon = Q(0.1);
      if (Util::abs(g_user.vx) < epsilon) g_user.vx = 0;
      if (Util::abs(g_user.vy) < epsilon) g_user.vy = 0;
    }

    // move user sprite to user location
    g_spr.origin(g_user.sprite, Q.floor(g_user.x), Q.floor(g_user.y) - g_grab.placing);

    // show grabbed item
    if (g_grab.item) {
      if (g_grab.grabbing) {
        g_spr.origin(
          g_grab.sprite,
          Q.floor(g_user.x) + (10 - g_grab.grabbing) * g_grab.dir,
          Q.floor(g_user.y) - g_grab.grabbing
        );
        g_grab.grabbing++;
        if (g_grab.grabbing > 10) {
          g_spr.priority(g_grab.sprite, 2);
          g_grab.grabbing = 0;
        }
      } else if (g_grab.placing) {
        g_spr.origin(
          g_grab.sprite,
          g_user.gx * 10,
          Q.floor(g_user.y) - 10 + g_grab.placing
        );
        g_grab.placing += 2;
        if (g_grab.placing > 10) {
          // place item
          g_grab.placing = 0;
          map_logic_set_type(g_user.gx, g_user.gy - 1, g_grab.item);
          g_user.y -= Q(10);
          g_grab.item = 0;
          g_spr.show(g_grab.sprite, false);
        }
      } else {
        g_spr.origin(g_grab.sprite, Q.floor(g_user.x), Q.floor(g_user.y) - 10);
      }
    }

    // show cursors
    g_spr.show(s_cursor, false);
    g_spr.show(s_place, false);
    if (g_user.grounded && !g_grab.grabbing && !g_grab.placing) {
      if (g_grab.item) {
        g_spr.show(s_place, true);
        g_spr.origin(s_place, g_user.gx * 10, g_user.gy * 10);
      } else if (g_pushed) {
        int tx = Util::div<10>(Q.floor(g_user.x));
        int ty = Util::div<10>(Q.floor(g_user.y));
        MapLogicFlags flag = map_logic_tile(tx + g_pushed, ty);
        if (flag.grab()) {
          g_spr.origin(s_cursor, (tx + g_pushed) * 10, ty * 10);
          g_spr.show(s_cursor, true);
        }
      }
    }

    { // move camera towards user
      int targetX = g_user.x - Q(115);
      int targetY = g_user.y - (g_user.grounded ? Q(106) : Q(97));
      if (g_camera.followSnap) {
        g_camera.followX = targetX;
        g_camera.followY = targetY;
        g_camera.followSnap = false;
      } else {
        int cameraLag = Q(0.11);
        int cameraDist = Util::max(
          Util::abs(g_camera.followX - targetX),
          Util::abs(g_camera.followY - targetY)
        );
        if (cameraDist > Q(50)) { // if we're really far away, then move faster
          cameraLag = Q.mul(cameraLag, Q(1) + Util::div<10>(cameraDist - Q(50)));
        }
        g_camera.followX -= Q.mul(g_camera.followX - targetX, cameraLag);
        g_camera.followY -= Q.mul(g_camera.followY - targetY, cameraLag);
      }
      camera_set(g_camera.followX, g_camera.followY);
    }

    nextframe();
  }
  return 0;
}

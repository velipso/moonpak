// SPDX-License-Identifier: 0BSD
#include "main.hpp"
#include "types/Player.hpp"
#include "data/palette.hpp"
#include "data/spritesheets/hero.8x8.hpp"
#include "data/tilesheets/tiles.8x8.hpp"
#include "data/animations.hpp"
#include "data/songs/demo.hpp"
#include "data/sfxs/Sfx.hpp"

Oam g_oam;
VramObj g_vramObj;
Spr g_spr;
Snd g_snd(2, 4);
inline constexpr Fix Q{6};

static Player g_player = {0};

static void irq_vblank() {
  Inp::update();
  Reg::IME::set(1); // allow sound engine to interrupt vblank
  Rnd::update();
  g_oam.copy(); // copy shadow OAM to real OAM
  g_spr.copy(); // copy queued tiles to VRAM
}

static inline void nextframe() {
  //g_spr.world(worldX, worldY); // set world coordinates (if used)
  g_spr.tick(); // advance animations
  Swi::vblankIntrWait();
  g_snd.copy(); // process sound engine
}

extern "C" int main() {
  Irq::init();
  Irq::vblank(irq_vblank);

  Reg::DISPCNT::write()
    .mode(0)
    .bg0(1)
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

  memcpy32((void *)0x06000000, dataTilesheetsTiles8x8, dataTilesheetsTiles8x8Size);
  Reg::BG0CNT::write()
    .tileBase(0)
    .is256(1)
    .mapBase(4)
    .done();

  volatile uint16_t *map = (volatile uint16_t *)(0x06000000 + 0x800 * 4);
  for (int ty = 0; ty < 32; ty++) {
    for (int tx = 0; tx < 32; tx++, map++) {
      if (ty < 11) {
        *map = 0;
      } else {
        *map = (ty & 1) ? ((tx & 1) ? 3 : 2) : ((tx & 1) ? 7 : 6);
      }
    }
  }

  g_snd.loadSong(0, dataSongsDemo, 0);
  g_snd.masterVolume = 16; // out of 16
  g_snd.songVolume = 8; // out of 16
  g_snd.sfxVolume = 8; // out of 16

  g_player.sprite(g_spr.alloc(8, 8, 0, true));

  SprEntry s_player = g_spr.entry(g_player.sprite());

  s_player
    .show(true)
    .anim(Anim::heroIdle);

  // player position is fixed point
  g_player.x(Q(120));
  g_player.y(Q(80));

  for (;;) {
    // update X movement
    int ndx = Util::clamp(
      g_player.dx() + Inp::dx() * Q(0.25),
      Q(-2.5),
      Q(2.5)
    );
    if (Inp::dx() == 0) {
      ndx >>= 1;
    }
    g_player.dx(ndx);

    // process jump
    if (Inp::A.hit() && g_player.grounded()) {
      g_player.dy(Q(-4.7));
      g_player.grounded(0);
      s_player.anim(Anim::heroUp);
      g_snd.playSfx(Sfx::jump, Sfx::jumpSize, 0);
    } else {
      int ndy = g_player.dy() + Q(0.25);
      if (g_player.dy() < 0 && ndy >= 0) {
        // transition at peak of jump
        s_player.anim(Anim::heroDown);
      }
      g_player.dy(ndy);
    }

    // update player X position
    int nx = g_player.x() + g_player.dx();
    if (nx < 0) {
      nx = 0;
      if (g_player.dx() < 0) g_player.dx(0);
    } else if (nx > Q(240 - 8)) {
      nx = Q(240 - 8);
      if (g_player.dx() > 0) g_player.dx(0);
    }
    g_player.x(nx);

    // update player Y position
    int ny = g_player.y() + g_player.dy();
    if (ny > Q(80)) {
      ny = Q(80);
      if (!g_player.grounded()) {
        s_player.anim(Anim::heroIdle);
        g_player.grounded(1);
      }
      if (g_player.dy() > 0) g_player.dy(0);
    }
    g_player.y(ny);

    // update player sprite origin
    s_player.origin(Q.floor(g_player.x()), Q.floor(g_player.y()));

    nextframe();
  }
  return 0;
}

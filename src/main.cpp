// SPDX-License-Identifier: 0BSD
#include "main.hpp"
#include "types/Player.hpp"
#include "data/palette.hpp"
#include "data/spritesheets/digits.8x8.hpp"
#include "data/animations.hpp"
#include "data/songs/outro.hpp"
#include "data/songs/basic.hpp"

Oam g_oam;
VramObj g_vramObj;
Spr g_spr;
Snd g_snd(2, 4);

static void irq_vblank() {
  g_oam.copy();
  g_spr.copy();
}

static inline void nextframe() {
  g_spr.tick();
  Swi::vblankIntrWait();
  g_snd.copy();
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

  *((volatile uint16_t *)0x05000000) = 0xf789;

  g_snd.loadSong(0, dataSongsOutro, 0);

  for (;;) {
    // game logic
    nextframe();
  }
  return 0;
}

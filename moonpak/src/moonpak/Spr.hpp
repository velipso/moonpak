// SPDX-License-Identifier: 0BSD
#pragma once
#include "moonpak/Oam.hpp"
#include "moonpak/VramObj.hpp"
#include <stdint.h>

namespace moonpak {
#include "moonpak/types/Spr.hpp"

struct SprCopy {
  int handle;
  const uint8_t *src;
};

struct SprEntry;

struct Spr {
  static Spr *global;
  uint32_t avail[4];
  SprEntryData entries[128];
  u32 seed;
  int worldXValue;
  int worldYValue;
  SprCopy *copyList;
  int copyListSize;
  int copyListTotal;

  Spr &reset();
  Spr() : copyList(nullptr), copyListSize(0), copyListTotal(0) { Spr::global = this; reset(); }
  int8_t alloc(int width, int height, uint8_t priority, bool is256);
  int8_t clone(int8_t handle);
  bool isEmpty();
  Spr &free(int8_t handle);
  int tileWidth(int8_t handle);
  int tileHeight(int8_t handle);
  Spr &queueCopyTiles(int8_t handle, const uint8_t *src);
  Spr &copyTiles(int8_t handle, const uint8_t *src);
  Spr &copy();
  Spr &tick();

  Spr &queueCopyTiles(int8_t handle, const uint8_t *src, int frame) {
    int bytes = frame * tileWidth(handle) * tileHeight(handle) * (is256(handle) ? 64 : 32);
    return queueCopyTiles(handle, src + bytes);
  }

  Spr &copyTiles(int8_t handle, const uint8_t *src, int frame) {
    int bytes = frame * tileWidth(handle) * tileHeight(handle) * (is256(handle) ? 64 : 32);
    return copyTiles(handle, src + bytes);
  }

  int width(int8_t handle) {
    return tileWidth(handle) << 3;
  }

  int height(int8_t handle) {
    return tileHeight(handle) << 3;
  }

  bool is256(int8_t handle) {
    return Oam::global->is256(entries[handle].oamHandle1());
  }

  bool show(int8_t handle) {
    return Oam::global->show(entries[handle].oamHandle1());
  }

  Spr &show(int8_t handle, bool value) {
    SprEntryData &e = entries[handle];
    Oam::global->show(e.oamHandle1(), value);
    if (e.oamHandle2() >= 0) {
      Oam::global->show(e.oamHandle2(), value);
    }
    return *this;
  }

  Spr &stop(int8_t handle) {
    entries[handle].pc(0); // global STOP
    return *this;
  }

  Spr &destroy(int8_t handle) {
    entries[handle].pc(1); // global DESTROY
    return *this;
  }

  int worldX() {
    return worldXValue;
  }

  Spr &worldX(int x) {
    worldXValue = x;
    for (int i = 0; i < 128; i++) {
      SprEntryData &e = entries[i];
      if (e.oamHandle1() >= 0 && e.worldSpace()) e.xyDirty(1);
    }
    return *this;
  }

  int worldY() {
    return worldYValue;
  }

  Spr &worldY(int y) {
    worldYValue = y;
    for (int i = 0; i < 128; i++) {
      SprEntryData &e = entries[i];
      if (e.oamHandle1() >= 0 && e.worldSpace()) e.xyDirty(1);
    }
    return *this;
  }

  Spr &world(int x, int y) {
    worldXValue = x;
    worldYValue = y;
    for (int i = 0; i < 128; i++) {
      SprEntryData &e = entries[i];
      if (e.oamHandle1() >= 0 && e.worldSpace()) e.xyDirty(1);
    }
    return *this;
  }

  int originX(int8_t handle) {
    return entries[handle].originX();
  }

  Spr &originX(int8_t handle, int x) {
    entries[handle].originX(x).xyDirty(1);
    return *this;
  }

  int originY(int8_t handle) {
    return entries[handle].originY();
  }

  Spr &originY(int8_t handle, int y) {
    entries[handle].originY(y).xyDirty(1);
    return *this;
  }

  Spr &origin(int8_t handle, int x, int y) {
    entries[handle].originX(x).originY(y).xyDirty(1);
    return *this;
  }

  Spr &anim(int8_t handle, u32 pc) {
    entries[handle].pc(pc);
    return *this;
  }

  uint8_t priority(int8_t handle) {
    return Oam::global->priority(entries[handle].oamHandle1());
  }

  Spr &priority(int8_t handle, uint8_t priority) {
    SprEntryData &e = entries[handle];
    Oam::global->priority(e.oamHandle1(), priority);
    if (e.oamHandle2() >= 0) {
      Oam::global->priority(e.oamHandle2(), priority);
    }
    return *this;
  }

  SprEntryData &entryData(int8_t handle) {
    return entries[handle];
  }

  inline SprEntry entry(int8_t handle);

#ifdef TESTS
  static int test(bool verbose);
#endif
};

struct SprEntry {
  int8_t handle;

  int8_t clone() { return Spr::global->clone(handle); }
  SprEntry &free() { Spr::global->free(handle); return *this; }
  int tileWidth() { return Spr::global->tileWidth(handle); }
  int tileHeight() { return Spr::global->tileHeight(handle); }
  SprEntry &queueCopyTiles(const uint8_t *src) {
    Spr::global->queueCopyTiles(handle, src); return *this;
  }
  SprEntry &copyTiles(const uint8_t *src) { Spr::global->copyTiles(handle, src); return *this; }
  SprEntry &queueCopyTiles(const uint8_t *src, int frame) {
    Spr::global->queueCopyTiles(handle, src, frame);
    return *this;
  }
  SprEntry &copyTiles(const uint8_t *src, int frame) {
    Spr::global->copyTiles(handle, src, frame);
    return *this;
  }
  int width() { return Spr::global->width(handle); }
  int height() { return Spr::global->height(handle); }
  bool is256() { return Spr::global->is256(handle); }
  bool show() { return Spr::global->show(handle); }
  SprEntry &show(bool value) { Spr::global->show(handle, value); return *this; }
  SprEntry &stop() { Spr::global->stop(handle); return *this; }
  SprEntry &destroy() { Spr::global->destroy(handle); return *this; }
  int originX() { return Spr::global->originX(handle); }
  SprEntry &originX(int x) { Spr::global->originX(handle, x); return *this; }
  int originY() { return Spr::global->originY(handle); }
  SprEntry &originY(int y) { Spr::global->originY(handle, y); return *this; }
  SprEntry &origin(int x, int y) { Spr::global->origin(handle, x, y); return *this; }
  SprEntry &anim(u32 pc) { Spr::global->anim(handle, pc); return *this; }
  uint8_t priority() { return Spr::global->priority(handle); }
  SprEntry &priority(uint8_t priority) { Spr::global->priority(handle, priority); return *this; }
};

inline SprEntry Spr::entry(int8_t handle) {
  return { handle };
}

} // moonpak

// SPDX-License-Identifier: 0BSD
//
// Simple input wrapper.
//
// Inside vblank:
//
// Inp::update();
//
// Then anywhere else:
//
// Inp::A.set();     // is A current pressed?
// Inp::A.hit();     // was A pressed this frame?
// Inp::A.rel();     // was A released this frame?
// Inp::A.changed(); // did A change this frame?
//
// Button naming:
//
// Inp::A
// Inp::B
// Inp::Select
// Inp::Start
// Inp::Right
// Inp::Left
// Inp::Up
// Inp::Down
// Inp::R
// Inp::L
//
#pragma once
#include "gba/Reg.hpp"
#include <stdint.h>

namespace Inp {
  extern uint32_t value;

  template<int Shift>
  struct Button {
    bool changed() {
      return (Inp::value & (1u << (Shift + 16))) != 0;
    }

    bool set() {
      return (Inp::value & (1u << Shift)) != 0;
    }

    bool rel() {
      return changed() && !set();
    }

    bool hit() {
      return changed() && set();
    }
  };

  inline constexpr Button<0> A;
  inline constexpr Button<1> B;
  inline constexpr Button<2> Select;
  inline constexpr Button<3> Start;
  inline constexpr Button<4> Right;
  inline constexpr Button<5> Left;
  inline constexpr Button<6> Up;
  inline constexpr Button<7> Down;
  inline constexpr Button<8> R;
  inline constexpr Button<9> L;
  inline constexpr Button<10> btn10;
  inline constexpr Button<11> btn11;
  inline constexpr Button<12> btn12;
  inline constexpr Button<13> btn13;
  inline constexpr Button<14> btn14;
  inline constexpr Button<15> btn15;

  inline void update(uint16_t current) {
    value = ((value ^ current) << 16) | current;
  }

  inline void update() {
    update(Reg::KEYINPUT::get() ^ 0x3ff);
  }
}

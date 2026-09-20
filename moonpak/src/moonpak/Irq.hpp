// SPDX-License-Identifier: 0BSD
#pragma once

namespace moonpak {

namespace Irq {
  extern "C" {
    void moonpak_irq_init();
    extern void (*moonpak_irq_vblank)();
    extern void (*moonpak_irq_hblank)();
    extern void (*moonpak_irq_vcount)();
    extern void (*moonpak_irq_timer0)();
    extern void (*moonpak_irq_timer1)();
    extern void (*moonpak_irq_timer2)();
    extern void (*moonpak_irq_timer3)();
    extern void (*moonpak_irq_serial)();
    extern void (*moonpak_irq_dma0)();
    extern void (*moonpak_irq_dma1)();
    extern void (*moonpak_irq_dma2)();
    extern void (*moonpak_irq_dma3)();
    extern void (*moonpak_irq_keypad)();
    extern void (*moonpak_irq_gamepak)();
  }

  static inline void init() {
    moonpak_irq_init();
  }

  static inline void (*vblank())() {
    return moonpak_irq_vblank;
  }

  static inline void vblank(void (*f)()) {
    moonpak_irq_vblank = f;
  }

  static inline void (*hblank())() {
    return moonpak_irq_hblank;
  }

  static inline void hblank(void (*f)()) {
    moonpak_irq_hblank = f;
  }

  static inline void (*vcount())() {
    return moonpak_irq_vcount;
  }

  static inline void vcount(void (*f)()) {
    moonpak_irq_vcount = f;
  }

  static inline void (*timer0())() {
    return moonpak_irq_timer0;
  }

  static inline void timer0(void (*f)()) {
    moonpak_irq_timer0 = f;
  }

  static inline void (*timer1())() {
    return moonpak_irq_timer1;
  }

  static inline void timer1(void (*f)()) {
    moonpak_irq_timer1 = f;
  }

  static inline void (*timer2())() {
    return moonpak_irq_timer2;
  }

  static inline void timer2(void (*f)()) {
    moonpak_irq_timer2 = f;
  }

  static inline void (*timer3())() {
    return moonpak_irq_timer3;
  }

  static inline void timer3(void (*f)()) {
    moonpak_irq_timer3 = f;
  }

  static inline void (*serial())() {
    return moonpak_irq_serial;
  }

  static inline void serial(void (*f)()) {
    moonpak_irq_serial = f;
  }

  static inline void (*dma0())() {
    return moonpak_irq_dma0;
  }

  static inline void dma0(void (*f)()) {
    moonpak_irq_dma0 = f;
  }

  static inline void (*dma1())() {
    return moonpak_irq_dma1;
  }

  static inline void dma1(void (*f)()) {
    moonpak_irq_dma1 = f;
  }

  static inline void (*dma2())() {
    return moonpak_irq_dma2;
  }

  static inline void dma2(void (*f)()) {
    moonpak_irq_dma2 = f;
  }

  static inline void (*dma3())() {
    return moonpak_irq_dma3;
  }

  static inline void dma3(void (*f)()) {
    moonpak_irq_dma3 = f;
  }

  static inline void (*keypad())() {
    return moonpak_irq_keypad;
  }

  static inline void keypad(void (*f)()) {
    moonpak_irq_keypad = f;
  }

  static inline void (*gamepak())() {
    return moonpak_irq_gamepak;
  }

  static inline void gamepak(void (*f)()) {
    moonpak_irq_gamepak = f;
  }
}

} // moonpak

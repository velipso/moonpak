// SPDX-License-Identifier: 0BSD
define(() => {
  const sprHero = () => {
    spritesheet('dataSpritesheetsHero8x8');
    worldSpace(true);
    local(-3, -3);
  }
  animation('heroNW', () => {
    sprHero();
    copy(0);
  });
  animation('heroN', () => {
    sprHero();
    copy(1);
  });
  animation('heroNE', () => {
    sprHero();
    copy(2);
  });
  animation('heroW', () => {
    sprHero();
    copy(3);
  });
  animation('heroC', () => {
    sprHero();
    copy(4);
  });
  animation('heroE', () => {
    sprHero();
    copy(5);
  });
  animation('heroSW', () => {
    sprHero();
    copy(6);
  });
  animation('heroS', () => {
    sprHero();
    copy(7);
  });
  animation('heroSE', () => {
    sprHero();
    copy(8);
  });
  animation('heroDead', () => {
    sprHero();
    copy(9);
  });
});

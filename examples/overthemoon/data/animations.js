// SPDX-License-Identifier: 0BSD
define(() => {
  const animHero = (name, index) => {
    animation(name, () => {
      spritesheet('dataSpritesheetsHero8x8');
      worldSpace(true);
      local(-3, -3);
      copy(index);
    });
  }
  animHero('heroNW', 0);
  animHero('heroN', 1);
  animHero('heroNE', 2);
  animHero('heroW', 3);
  animHero('heroC', 4);
  animHero('heroE', 5);
  animHero('heroSW', 6);
  animHero('heroS', 7);
  animHero('heroSE', 8);
  animHero('heroDead', 9);

  animation('cursor', () => {
    spritesheet('dataSpritesheetsCursor8x8');
    worldSpace(true);
    forever(() => {
      for (let i = 0; i < 9; i++) {
        copy(i);
        wait(3);
      }
    });
  });

  const animItem = (name, index) => {
    animation(name, () => {
      spritesheet('dataSpritesheetsItems8x8');
      worldSpace(true);
      local(-3, -3);
      copy(index);
    });
  };
  animItem('itemFuse', 0);
  animItem('itemAnchor', 1);
  animItem('itemJetpack', 2);
  animItem('itemBalloon', 3);

  animation('place', () => {
    spritesheet('dataSpritesheetsPlace8x8');
    worldSpace(true);
    local(-3, -3);
    forever(() => {
      copy(0); wait(4);
      copy(1); wait(4);
      copy(2); wait(4);
      copy(3); wait(4);
      copy(2); wait(4);
      copy(1); wait(4);
    });
  });
});

// SPDX-License-Identifier: 0BSD
define(() => {
  animation('heroIdle', () => {
    spritesheet('dataSpritesheetsHero8x8');
    copy(0);
    forever(() => {
      wait(20);
      hFlip(true);
      wait(20);
      hFlip(false);
    });
  });

  animation('heroUp', () => {
    copy(3);
  });

  animation('heroDown', () => {
    copy(2);
  });
});

// SPDX-License-Identifier: 0BSD
    .section    .iwram, "ax"
    .global     snd_dma_read_section
    .global     snd_dma_swap
    .cpu        arm7tdmi
    .arm

snd_dma_read_section:
    .space 4
snd_dma_swap:
    bx    lr
    .align 4
    .pool
    .end

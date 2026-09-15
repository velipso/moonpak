// SPDX-License-Identifier: 0BSD
    .section    .iwram, "ax"
    .global     sndRenderNoiseSet
    .global     sndRenderNoiseAdd
    .global     sndQuantize8
    .cpu        arm7tdmi
    .arm

.macro NOISE_STEP
    adds    r4, r4, r6         @ p += dphase, C = overflow
    movcss  r5, r5, lsr #1     @ if overflow: s >>= 1, C = old s bit 0
    eorcs   r5, r5, r8         @ if old bit 0: s ^= 0x80200003
.endm

sndRenderNoiseSet:
    cmp     r1, #0
    bxeq    lr
    push    {r4-r9}
    ldr     r6, [sp, #24]      @ dphase
    ldr     r7, [sp, #28]      @ state
    ldr     r4, [r3]           @ p = *phase
    ldr     r5, [r7]           @ s = *state
    ldr     r8, =0x80200003
1:
    mov     r9, r5, asr #16    @ r9 = s >> 16
    mul     r12, r9, r2        @ r12 = r9 * r2 (volume)
    mov     r12, r12, asr #12  @ r12 >>= 12 (shift 8 on host, shift 12 on GBA for more headroom)
    strh    r12, [r0], #2      @ *out++ = r12
    NOISE_STEP

    mov     r9, r5, asr #16
    mul     r12, r9, r2
    mov     r12, r12, asr #12
    strh    r12, [r0], #2
    NOISE_STEP

    subs    r1, r1, #2         @ samples -= 2
    bne     1b

    str     r4, [r3]           @ *phase = p
    str     r5, [r7]           @ *state = s
    pop     {r4-r9}
    bx      lr

sndRenderNoiseAdd:
    cmp     r1, #0
    bxeq    lr
    push    {r4-r9}
    ldr     r6, [sp, #24]      @ dphase
    ldr     r7, [sp, #28]      @ state
    ldr     r4, [r3]           @ p = *phase
    ldr     r5, [r7]           @ s = *state
    ldr     r8, =0x80200003
1:
    mov     r9, r5, asr #16    @ r9 = s >> 16
    mul     r12, r9, r2        @ r12 = r9 * r2 (volume)
    mov     r12, r12, asr #12  @ r12 >>= 12 (shift 8 on host, shift 12 on GBA for more headroom)
    ldrsh   r9, [r0]           @ r9 = *out
    add     r12, r12, r9       @ r12 += r9
    strh    r12, [r0], #2      @ *out++ = r12
    NOISE_STEP

    mov     r9, r5, asr #16
    mul     r12, r9, r2
    mov     r12, r12, asr #12
    ldrsh   r9, [r0]
    add     r12, r12, r9
    strh    r12, [r0], #2
    NOISE_STEP

    subs    r1, r1, #2        @ samples -= 2
    bne     1b

    str     r4, [r3]          @ *phase = p
    str     r5, [r7]          @ *state = s
    pop     {r4-r9}
    bx      lr

sndQuantize8:
    cmp     r1, #0
    bxeq    lr
    push    {r4, lr}
    mov     r4, #0x7f
1:
    ldrsh   r3, [r2], #2      @ sample = *bufferTemp++
    mov     r3, r3, asr #4    @ 12-bit -> 8-bit

    @ clamp r3 to -128..127 (magic)
    mov     r12, r3, lsl #24
    cmp     r3, r12, asr #24
    eorne   r3, r4, r3, asr #32

    strb    r3, [r0], #1      @ *bufferDMA++ = sample

    subs    r1, r1, #1
    bne     1b

    pop     {r4, pc}

    .balign 4
    .pool
    .end

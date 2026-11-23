.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001080
    li a2, 0x8000101C # 0x80001000 + 28
    li a3, 0x80001040

    # Initialize 8 32-bit integers at 0x80001000
    li t0, 100
    sw t0, 0(a0)
    li t0, 200
    sw t0, 4(a0)
    li t0, 300
    sw t0, 8(a0)
    li t0, 400
    sw t0, 12(a0)
    li t0, 500
    sw t0, 16(a0)
    li t0, 600
    sw t0, 20(a0)
    li t0, 700
    sw t0, 24(a0)
    li t0, 800
    sw t0, 28(a0)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    # 1. Positive stride: +8 bytes (load elements 0, 2, 4, 6)
    li t3, 8
    vlse32.v v1, (a0), t3

    # Store with stride 16 bytes to a1
    li t4, 16
    vsse32.v v1, (a1), t4

    # 2. Negative stride: -8 bytes from a2 (load elements 7, 5, 3, 1)
    li t5, -8
    vlse32.v v2, (a2), t5

    # Store unit-stride to a3
    vse32.v v2, (a3)

test_done:
    ebreak

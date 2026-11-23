.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001010

    # Store 4 32-bit words for v1
    li t0, 1
    sw t0, 0(a0)
    li t0, -16
    sw t0, 4(a0)
    li t0, 0x12345678
    sw t0, 8(a0)
    li t0, -1
    sw t0, 12(a0)

    # Store 4 32-bit words for v2 (shift amounts)
    li t0, 4
    sw t0, 0(a1)
    li t0, 33 # 33 % 32 = 1
    sw t0, 4(a1)
    li t0, 1
    sw t0, 8(a1)
    li t0, 31
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    li t3, 3
    li t4, 36 # 36 % 32 = 4

    vsll.vv v3, v1, v2
    vsrl.vv v4, v1, v2
    vsra.vv v5, v1, v2

    vsll.vx v6, v1, t3
    vsrl.vx v7, v1, t4
    vsra.vx v8, v1, t3

    vsll.vi v9, v1, 2
    vsrl.vi v10, v1, 3
    vsra.vi v11, v1, 4

test_done:
    ebreak

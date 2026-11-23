.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001100
    li a2, 0x80001120
    li a3, 0x80001140

    # Fill 16 bytes at 0x80001000: 1, 2, ..., 16
    li t0, 0x04030201
    sw t0, 0(a0)
    li t0, 0x08070605
    sw t0, 4(a0)
    li t0, 0x0C0B0A09
    sw t0, 8(a0)
    li t0, 0x100F0E0D
    sw t0, 12(a0)

    # 1. Test e8 (16 elements)
    li t1, 16
    vsetvli t0, t1, e8, m1, ta, ma
    vle8.v v1, (a0)
    vadd.vi v2, v1, 1
    vse8.v v2, (a1)

    # 2. Test e16 (8 elements)
    li t1, 8
    vsetvli t0, t1, e16, m1, ta, ma
    vle16.v v3, (a0)
    vadd.vi v4, v3, 2
    vse16.v v4, (a2)

    # 3. Test e64 (2 elements)
    li t1, 2
    vsetvli t0, t1, e64, m1, ta, ma
    vle64.v v5, (a0)
    vadd.vi v6, v5, 3
    vse64.v v6, (a3)

test_done:
    ebreak

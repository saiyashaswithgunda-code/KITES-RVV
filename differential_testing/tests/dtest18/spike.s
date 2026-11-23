.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001010
    li a2, 0x80001020

    # Store 4 32-bit words for v1
    li t0, 10
    sw t0, 0(a0)
    li t0, 20
    sw t0, 4(a0)
    li t0, 30
    sw t0, 8(a0)
    li t0, 40
    sw t0, 12(a0)

    # Store 4 32-bit words for v2
    li t0, 1
    sw t0, 0(a1)
    li t0, 2
    sw t0, 4(a1)
    li t0, 3
    sw t0, 8(a1)
    li t0, 4
    sw t0, 12(a1)

    # Store 4 32-bit words for v3
    li t0, 5
    sw t0, 0(a2)
    li t0, 10
    sw t0, 4(a2)
    li t0, 15
    sw t0, 8(a2)
    li t0, 20
    sw t0, 12(a2)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)
    vle32.v v3, (a2)

    # 1. vd == vs2 (v1 = v1 + v2)
    vadd.vv v1, v1, v2

    # 2. vd == vs1 (v2 = v1 - v2)
    vsub.vv v2, v1, v2

    # 3. vd == vs1 == vs2 (v3 = v3 + v3)
    vadd.vv v3, v3, v3

    # 4. vd overwrites source vector with comparison mask
    vmseq.vv v1, v1, v2

    # 5. vd == v0 (comparison writing directly into v0)
    vmseq.vv v0, v2, v3

test_done:
    ebreak

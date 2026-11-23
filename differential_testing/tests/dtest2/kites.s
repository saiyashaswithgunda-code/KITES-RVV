    # Memory address for loading inputs
    li a0, 0x80001000

    # Load 4 elements for v1: 100, -50, 2147483647 (0x7FFFFFFF), 0
    li t0, 100
    sw t0, 0(a0)
    li t0, -50
    sw t0, 4(a0)
    li t0, 2147483647
    sw t0, 8(a0)
    li t0, 0
    sw t0, 12(a0)

    # Load 4 elements for v2: 25, 30, 1, -1 (0xFFFFFFFF)
    li t0, 25
    sw t0, 16(a0)
    li t0, 30
    sw t0, 20(a0)
    li t0, 1
    sw t0, 24(a0)
    li t0, -1
    sw t0, 28(a0)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    addi a1, a0, 16
    vle32.v v2, (a1)

    vadd.vv v3, v1, v2
    vsub.vv v4, v1, v2
    vmin.vv v5, v1, v2
    vminu.vv v6, v1, v2
    vmax.vv v7, v1, v2
    vmaxu.vv v8, v1, v2

test_done:
    ebreak

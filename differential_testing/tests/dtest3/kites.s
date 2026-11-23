    # Memory address for loading inputs
    li a0, 0x80001000
    li a1, 0

    # Load 8 16-bit elements for v1
    li t0, 10
    sh t0, 0(a0)
    li t0, -20
    sh t0, 2(a0)
    li t0, 300
    sh t0, 4(a0)
    li t0, -4000
    sh t0, 6(a0)
    li t0, 32767
    sh t0, 8(a0)
    li t0, -32768
    sh t0, 10(a0)
    li t0, 0
    sh t0, 12(a0)
    li t0, 50
    sh t0, 14(a0)

    # Configure vector: 8 elements, 16-bit, LMUL=1
    li t1, 8
    vsetvli t0, t1, e16, m1, ta, ma

    vle16.v v1, (a0)

    li t3, 15
    li t4, -25

    vadd.vx v2, v1, t3
    vsub.vx v3, v1, t3
    vrsub.vx v4, v1, t3
    vmin.vx v5, v1, t4
    vminu.vx v6, v1, t3
    vmax.vx v7, v1, t4
    vmaxu.vx v8, v1, t3

test_done:
    ebreak

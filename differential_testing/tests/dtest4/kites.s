    # Memory address for loading inputs
    li a0, 0x80001000
    li a1, 0

    # Store 16 bytes for v1
    li t0, 0
    sb t0, 0(a0)
    li t0, 1
    sb t0, 1(a0)
    li t0, 2
    sb t0, 2(a0)
    li t0, 10
    sb t0, 3(a0)
    li t0, 15
    sb t0, 4(a0)
    li t0, 16
    sb t0, 5(a0)
    li t0, 127
    sb t0, 6(a0)
    li t0, -128
    sb t0, 7(a0)
    li t0, -120
    sb t0, 8(a0)
    li t0, -1
    sb t0, 9(a0)
    li t0, -5
    sb t0, 10(a0)
    li t0, 50
    sb t0, 11(a0)
    li t0, 75
    sb t0, 12(a0)
    li t0, 100
    sb t0, 13(a0)
    li t0, -50
    sb t0, 14(a0)
    li t0, 0
    sb t0, 15(a0)

    # Configure vector: 16 elements, 8-bit, LMUL=1
    li t1, 16
    vsetvli t0, t1, e8, m1, ta, ma

    vle8.v v1, (a0)

    vadd.vi v2, v1, 5
    vadd.vi v3, v1, -16
    vadd.vi v4, v1, 15
    vadd.vi v5, v1, 0
    vrsub.vi v6, v1, 0
    vrsub.vi v7, v1, 15
    vrsub.vi v8, v1, -16

test_done:
    ebreak

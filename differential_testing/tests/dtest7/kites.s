    li a0, 0x80001000
    li a1, 0x80001010

    # Store 4 32-bit words for v1
    li t0, 100000
    sw t0, 0(a0)
    li t0, -200000
    sw t0, 4(a0)
    li t0, 2147483647
    sw t0, 8(a0)
    li t0, -1
    sw t0, 12(a0)

    # Store 4 32-bit words for v2
    li t0, 300000
    sw t0, 0(a1)
    li t0, 400000
    sw t0, 4(a1)
    li t0, 2
    sw t0, 8(a1)
    li t0, -2147483648 # 0x80000000
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    li t3, -10
    li t4, 0x70000000

    vmul.vv v3, v1, v2
    vmulh.vv v4, v1, v2
    vmulhu.vv v5, v1, v2
    vmulhsu.vv v6, v1, v2

    vmul.vx v7, v1, t3
    vmulh.vx v8, v1, t3
    vmulhu.vx v9, v1, t4
    vmulhsu.vx v10, v1, t4

test_done:
    ebreak

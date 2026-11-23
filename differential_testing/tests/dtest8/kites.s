    li a0, 0x80001000
    li a1, 0x80001010

    # Store 4 32-bit words for v1
    li t0, 100
    sw t0, 0(a0)
    li t0, -2147483648 # INT_MIN
    sw t0, 4(a0)
    li t0, -50
    sw t0, 8(a0)
    li t0, 42
    sw t0, 12(a0)

    # Store 4 32-bit words for v2
    li t0, 7
    sw t0, 0(a1)
    li t0, -1          # triggers INT_MIN / -1 overflow
    sw t0, 4(a1)
    li t0, 6
    sw t0, 8(a1)
    li t0, 0           # triggers division by zero
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    li t3, 0
    li t4, -5

    vdiv.vv v3, v1, v2
    vdivu.vv v4, v1, v2
    vrem.vv v5, v1, v2
    vremu.vv v6, v1, v2

    vdiv.vx v7, v1, t4
    vdiv.vx v8, v1, t3
    vrem.vx v9, v1, t4
    vrem.vx v10, v1, t3

test_done:
    ebreak

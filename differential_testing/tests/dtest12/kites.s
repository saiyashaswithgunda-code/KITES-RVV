    li a0, 0x80001000
    li a1, 0x80001010

    # Store 4 32-bit words for v1
    li t0, 15
    sw t0, 0(a0)
    li t0, -30
    sw t0, 4(a0)
    li t0, 45
    sw t0, 8(a0)
    li t0, 10
    sw t0, 12(a0)

    # Store scalar accumulator for v2
    li t0, 5
    sw t0, 0(a1)
    li t0, 0
    sw t0, 4(a1)
    sw t0, 8(a1)
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1, tail undisturbed
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    vredsum.vs v3, v1, v2
    vredmax.vs v4, v1, v2
    vredmaxu.vs v5, v1, v2
    vredmin.vs v6, v1, v2
    vredminu.vs v7, v1, v2
    vredand.vs v8, v1, v2
    vredor.vs v9, v1, v2
    vredxor.vs v10, v1, v2

test_done:
    ebreak

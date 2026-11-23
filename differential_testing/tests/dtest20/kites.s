    # Dot Product: scalar = sum(A[i] * B[i]) for 7 elements (SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001020 # Address of Array B
    li a2, 0x80001050 # Address of intermediate products (7 words)
    li a3, 0x80001080 # Address of final dot product (1 word)

    # Initialize Array A (7 elements: 3, -5, 8, 0, -2, 10, -4)
    li t0, 3
    sw t0, 0(a0)
    li t0, -5
    sw t0, 4(a0)
    li t0, 8
    sw t0, 8(a0)
    li t0, 0
    sw t0, 12(a0)
    li t0, -2
    sw t0, 16(a0)
    li t0, 10
    sw t0, 20(a0)
    li t0, -4
    sw t0, 24(a0)

    # Initialize Array B (7 elements: -2, 4, 3, 7, -6, -1, 5)
    li t0, -2
    sw t0, 0(a1)
    li t0, 4
    sw t0, 4(a1)
    li t0, 3
    sw t0, 8(a1)
    li t0, 7
    sw t0, 12(a1)
    li t0, -6
    sw t0, 16(a1)
    li t0, -1
    sw t0, 20(a1)
    li t0, 5
    sw t0, 24(a1)

    # Initialize reduction accumulator v10 to 0
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu
    vxor.vv v10, v10, v10

    # Pointers and remaining elements
    mv t4, a0 # ptr A
    mv t5, a1 # ptr B
    mv t6, a2 # ptr products
    li s0, 7  # remaining elements

loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t4)
    vle32.v v2, (t5)
    vmul.vv v3, v1, v2
    vse32.v v3, (t6)

    # Accumulate into v10[0]
    vredsum.vs v10, v3, v10

    # Advance pointers by t0 * 4
    slli s1, t0, 2
    add t4, t4, s1
    add t5, t5, s1
    add t6, t6, s1
    sub s0, s0, t0
    bnez s0, loop

    # Store final scalar dot product from v10[0] to memory at a3
    vse32.v v10, (a3)

test_done:
    ebreak

    # Minimum and Maximum: Element-wise and Global (8 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001020 # Address of Array B
    li a2, 0x80001040 # Address of Element Min (C)
    li a3, 0x80001060 # Address of Element Max (D)
    li a4, 0x80001080 # Address of Global Min and Max

    # Initialize Array A (8 elements: -15, 42, -99, 120, 0, -3, 85, -200)
    li t0, -15
    sw t0, 0(a0)
    li t0, 42
    sw t0, 4(a0)
    li t0, -99
    sw t0, 8(a0)
    li t0, 120
    sw t0, 12(a0)
    li t0, 0
    sw t0, 16(a0)
    li t0, -3
    sw t0, 20(a0)
    li t0, 85
    sw t0, 24(a0)
    li t0, -200
    sw t0, 28(a0)

    # Initialize Array B (8 elements: 10, -50, 0, 75, -8, 25, 85, -150)
    li t0, 10
    sw t0, 0(a1)
    li t0, -50
    sw t0, 4(a1)
    li t0, 0
    sw t0, 8(a1)
    li t0, 75
    sw t0, 12(a1)
    li t0, -8
    sw t0, 16(a1)
    li t0, 25
    sw t0, 20(a1)
    li t0, 85
    sw t0, 24(a1)
    li t0, -150
    sw t0, 28(a1)

    # Initialize global min accumulator with A[0] (-15) and max with A[0] (-15)
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu
    lw t0, 0(a0)
    sw t0, 0(a4)
    vle32.v v10, (a4) # v10[0] = -15 for min
    vle32.v v11, (a4) # v11[0] = -15 for max

    mv t3, a0
    mv t4, a1
    mv t5, a2
    mv t6, a3
    li s0, 8 # remaining elements

min_max_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)

    # Element-wise min and max
    vmin.vv v3, v1, v2
    vmax.vv v4, v1, v2
    vse32.v v3, (t5)
    vse32.v v4, (t6)

    # Global min and max reduction
    vredmin.vs v10, v1, v10
    vredmax.vs v11, v1, v11

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    add t5, t5, s1
    add t6, t6, s1
    sub s0, s0, t0
    bnez s0, min_max_loop

    # Store global min at 0(a4) and global max at 4(a4)
    vse32.v v10, (a4)
    addi t0, a4, 4
    vse32.v v11, (t0)

test_done:
    ebreak

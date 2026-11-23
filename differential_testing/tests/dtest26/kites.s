    # Bitwise Vector Processing (8 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001020 # Address of Array B
    li a2, 0x80001040 # Address of AND Output
    li a3, 0x80001060 # Address of OR Output
    li a4, 0x80001080 # Address of XOR Output
    li a5, 0x800010A0 # Address of Reductions Output (AND, OR, XOR)

    # Initialize Array A (8 elements)
    li t0, 0x0F0F0F0F
    sw t0, 0(a0)
    li t0, 0x33333333
    sw t0, 4(a0)
    li t0, 0x55555555
    sw t0, 8(a0)
    li t0, 0x00FF00FF
    sw t0, 12(a0)
    li t0, 0xAAAAAAAA
    sw t0, 16(a0)
    li t0, 0xCCCCCCCC
    sw t0, 20(a0)
    li t0, 0xF0F0F0F0
    sw t0, 24(a0)
    li t0, 0xFF00FF00
    sw t0, 28(a0)

    # Initialize Array B (8 elements)
    li t0, -1 # 0xFFFFFFFF
    sw t0, 0(a1)
    li t0, 0x0F0F0F0F
    sw t0, 4(a1)
    li t0, 0xAAAAAAAA
    sw t0, 8(a1)
    li t0, 0x55555555
    sw t0, 12(a1)
    li t0, 0x33333333
    sw t0, 16(a1)
    li t0, 0x66666666
    sw t0, 20(a1)
    li t0, 0
    sw t0, 24(a1)
    li t0, 0x12345678
    sw t0, 28(a1)

    # Initialize reduction accumulators
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu
    li t0, -1
    sw t0, 0(a5)
    vle32.v v10, (a5) # v10[0] = 0xFFFFFFFF for AND-reduction
    li t0, 0
    sw t0, 0(a5)
    vle32.v v11, (a5) # v11[0] = 0 for OR-reduction
    vle32.v v12, (a5) # v12[0] = 0 for XOR-reduction

    mv t3, a0
    mv t4, a1
    mv t5, a2
    mv t6, a3
    mv s2, a4
    li s0, 8 # remaining elements

bitwise_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)

    # Element-wise operations
    vand.vv v3, v1, v2
    vor.vv  v4, v1, v2
    vxor.vv v5, v1, v2
    vse32.v v3, (t5)
    vse32.v v4, (t6)
    vse32.v v5, (s2)

    # Reductions over array A
    vredand.vs v10, v1, v10
    vredor.vs  v11, v1, v11
    vredxor.vs v12, v1, v12

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    add t5, t5, s1
    add t6, t6, s1
    add s2, s2, s1
    sub s0, s0, t0
    bnez s0, bitwise_loop

    # Store reduction results: AND at 0(a5), OR at 4(a5), XOR at 8(a5)
    vse32.v v10, (a5)
    addi t0, a5, 4
    vse32.v v11, (t0)
    addi t0, a5, 8
    vse32.v v12, (t0)

test_done:
    ebreak

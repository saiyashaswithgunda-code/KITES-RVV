    # Element-Wise Multiplication: Low and High halves (6 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001020 # Address of Array B
    li a2, 0x80001040 # Address of Array C (Low Product)
    li a3, 0x80001060 # Address of Array D (High Product)

    # Initialize Array A (6 elements)
    li t0, 100000
    sw t0, 0(a0)
    li t0, -200000
    sw t0, 4(a0)
    li t0, 300000
    sw t0, 8(a0)
    li t0, -400000
    sw t0, 12(a0)
    li t0, 500000
    sw t0, 16(a0)
    li t0, -600000
    sw t0, 20(a0)

    # Initialize Array B (6 elements)
    li t0, 70000
    sw t0, 0(a1)
    li t0, 80000
    sw t0, 4(a1)
    li t0, -90000
    sw t0, 8(a1)
    li t0, 100000
    sw t0, 12(a1)
    li t0, -110000
    sw t0, 16(a1)
    li t0, 120000
    sw t0, 20(a1)

    mv t3, a0
    mv t4, a1
    mv t5, a2
    mv t6, a3
    li s0, 6 # remaining elements

mul_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)
    vmul.vv v3, v1, v2
    vmulh.vv v4, v1, v2
    vse32.v v3, (t5)
    vse32.v v4, (t6)

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    add t5, t5, s1
    add t6, t6, s1
    sub s0, s0, t0
    bnez s0, mul_loop

test_done:
    ebreak

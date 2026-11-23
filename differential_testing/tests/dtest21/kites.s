    # Vector Addition and Subtraction: C[i] = A[i] + B[i], D[i] = A[i] - B[i] (10 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001030 # Address of Array B
    li a2, 0x80001060 # Address of Array C (A + B)
    li a3, 0x80001090 # Address of Array D (A - B)

    # Initialize Array A (10 elements)
    li t0, 100
    sw t0, 0(a0)
    li t0, -200
    sw t0, 4(a0)
    li t0, 300
    sw t0, 8(a0)
    li t0, -400
    sw t0, 12(a0)
    li t0, 500
    sw t0, 16(a0)
    li t0, -600
    sw t0, 20(a0)
    li t0, 700
    sw t0, 24(a0)
    li t0, -800
    sw t0, 28(a0)
    li t0, 900
    sw t0, 32(a0)
    li t0, -1000
    sw t0, 36(a0)

    # Initialize Array B (10 elements)
    li t0, 50
    sw t0, 0(a1)
    li t0, 60
    sw t0, 4(a1)
    li t0, -70
    sw t0, 8(a1)
    li t0, -80
    sw t0, 12(a1)
    li t0, 90
    sw t0, 16(a1)
    li t0, 100
    sw t0, 20(a1)
    li t0, -110
    sw t0, 24(a1)
    li t0, -120
    sw t0, 28(a1)
    li t0, 130
    sw t0, 32(a1)
    li t0, 140
    sw t0, 36(a1)

    mv t3, a0
    mv t4, a1
    mv t5, a2
    mv t6, a3
    li s0, 10 # remaining elements

add_sub_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)
    vadd.vv v3, v1, v2
    vsub.vv v4, v1, v2
    vse32.v v3, (t5)
    vse32.v v4, (t6)

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    add t5, t5, s1
    add t6, t6, s1
    sub s0, s0, t0
    bnez s0, add_sub_loop

test_done:
    ebreak

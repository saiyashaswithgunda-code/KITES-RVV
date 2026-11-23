    # Vector Scaling (SAXPY): Y[i] = A * X[i] + Y[i] (9 elements, SEW=32, scalar A = -3)
    li a0, 0x80001000 # Address of Array X
    li a1, 0x80001030 # Address of Array Y (Input & Output)

    # Initialize Array X (9 elements)
    li t0, 10
    sw t0, 0(a0)
    li t0, -20
    sw t0, 4(a0)
    li t0, 30
    sw t0, 8(a0)
    li t0, -40
    sw t0, 12(a0)
    li t0, 50
    sw t0, 16(a0)
    li t0, -60
    sw t0, 20(a0)
    li t0, 70
    sw t0, 24(a0)
    li t0, -80
    sw t0, 28(a0)
    li t0, 90
    sw t0, 32(a0)

    # Initialize Array Y (9 elements)
    li t0, 100
    sw t0, 0(a1)
    li t0, 200
    sw t0, 4(a1)
    li t0, 300
    sw t0, 8(a1)
    li t0, 400
    sw t0, 12(a1)
    li t0, 500
    sw t0, 16(a1)
    li t0, 600
    sw t0, 20(a1)
    li t0, 700
    sw t0, 24(a1)
    li t0, 800
    sw t0, 28(a1)
    li t0, 900
    sw t0, 32(a1)

    li t2, -3 # Scalar A
    mv t3, a0
    mv t4, a1
    li s0, 9  # remaining elements

saxpy_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)
    vmul.vx v3, v1, t2
    vadd.vv v4, v3, v2
    vse32.v v4, (t4)

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    sub s0, s0, t0
    bnez s0, saxpy_loop

test_done:
    ebreak

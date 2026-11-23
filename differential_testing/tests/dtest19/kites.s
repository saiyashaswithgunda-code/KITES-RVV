    # Matrix Multiplication: C(3x3) = A(3x3) * B(3x3)
    # SEW = 32, vl = 3 (nontrivial dimension < VLMAX=4)

    li a0, 0x80001000 # Address of Matrix A
    li a1, 0x80001030 # Address of Matrix B
    li a2, 0x80001060 # Address of Matrix C (Output)

    # Store Matrix A (row-major)
    # Row 0: 1, 2, -1
    li t0, 1
    sw t0, 0(a0)
    li t0, 2
    sw t0, 4(a0)
    li t0, -1
    sw t0, 8(a0)
    # Row 1: 3, 0, 4
    li t0, 3
    sw t0, 12(a0)
    li t0, 0
    sw t0, 16(a0)
    li t0, 4
    sw t0, 20(a0)
    # Row 2: -2, 5, 1
    li t0, -2
    sw t0, 24(a0)
    li t0, 5
    sw t0, 28(a0)
    li t0, 1
    sw t0, 32(a0)

    # Store Matrix B (row-major)
    # Row 0: 2, -1, 3
    li t0, 2
    sw t0, 0(a1)
    li t0, -1
    sw t0, 4(a1)
    li t0, 3
    sw t0, 8(a1)
    # Row 1: 0, 4, 1
    li t0, 0
    sw t0, 12(a1)
    li t0, 4
    sw t0, 16(a1)
    li t0, 1
    sw t0, 20(a1)
    # Row 2: 5, 2, -2
    li t0, 5
    sw t0, 24(a1)
    li t0, 2
    sw t0, 28(a1)
    li t0, -2
    sw t0, 32(a1)

    # Configure vector: 3 elements, 32-bit, LMUL=1, tail undisturbed
    li t1, 3
    vsetvli t0, t1, e32, m1, tu, mu

    # Load all 3 rows of B into v1, v2, v3
    vle32.v v1, (a1)
    addi t6, a1, 12
    vle32.v v2, (t6)
    addi t6, a1, 24
    vle32.v v3, (t6)

    # --- Compute Row 0 of C ---
    lw t3, 0(a0)      # A[0][0] = 1
    vmul.vx v10, v1, t3
    lw t3, 4(a0)      # A[0][1] = 2
    vmul.vx v11, v2, t3
    vadd.vv v10, v10, v11
    lw t3, 8(a0)      # A[0][2] = -1
    vmul.vx v12, v3, t3
    vadd.vv v10, v10, v12
    vse32.v v10, (a2) # Store C[0][*]

    # --- Compute Row 1 of C ---
    lw t3, 12(a0)     # A[1][0] = 3
    vmul.vx v10, v1, t3
    lw t3, 16(a0)     # A[1][1] = 0
    vmul.vx v11, v2, t3
    vadd.vv v10, v10, v11
    lw t3, 20(a0)     # A[1][2] = 4
    vmul.vx v12, v3, t3
    vadd.vv v10, v10, v12
    addi t6, a2, 12
    vse32.v v10, (t6) # Store C[1][*]

    # --- Compute Row 2 of C ---
    lw t3, 24(a0)     # A[2][0] = -2
    vmul.vx v10, v1, t3
    lw t3, 28(a0)     # A[2][1] = 5
    vmul.vx v11, v2, t3
    vadd.vv v10, v10, v11
    lw t3, 32(a0)     # A[2][2] = 1
    vmul.vx v12, v3, t3
    vadd.vv v10, v10, v12
    addi t6, a2, 24
    vse32.v v10, (t6) # Store C[2][*]

test_done:
    ebreak

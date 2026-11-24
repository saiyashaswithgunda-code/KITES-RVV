# Matrix Multiplication - Vector RV32IMV (KITES)
# Expected checksum in a0 (x10): -78 (0xFFFFFFB2)

.data
matmul_a:
.word -11, -4, 3, 10, -12, -5, 2, 9
.word -13, -6, 1, 8, -14, -7, 0, 7
.word 2, 9, -13, -6, 1, 8, -14, -7
.word 0, 7, 14, -8, -1, 6, 13, -9
.word -14, -7, 0, 7, 14, -8, -1, 6
.word 13, -9, -2, 5, 12, -10, -3, 4
.word -1, 6, 13, -9, -2, 5, 12, -10
.word -3, 4, 11, -11, -4, 3, 10, -12
.word 12, -10, -3, 4, 11, -11, -4, 3
.word 10, -12, -5, 2, 9, -13, -6, 1
.word -4, 3, 10, -12, -5, 2, 9, -13
.word -6, 1, 8, -14, -7, 0, 7, 14
.word 9, -13, -6, 1, 8, -14, -7, 0
.word 7, 14, -8, -1, 6, 13, -9, -2
.word -7, 0, 7, 14, -8, -1, 6, 13
.word -9, -2, 5, 12, -10, -3, 4, 11
.word 6, 13, -9, -2, 5, 12, -10, -3
.word 4, 11, -11, -4, 3, 10, -12, -5
.word -10, -3, 4, 11, -11, -4, 3, 10
.word -12, -5, 2, 9, -13, -6, 1, 8
.word 3, 10, -12, -5, 2, 9, -13, -6
.word 1, 8, -14, -7, 0, 7, 14, -8
.word -13, -6, 1, 8, -14, -7, 0, 7
.word 14, -8, -1, 6, 13, -9, -2, 5
.word 0, 7, 14, -8, -1, 6, 13, -9
.word -2, 5, 12, -10, -3, 4, 11, -11
.word 13, -9, -2, 5, 12, -10, -3, 4
.word 11, -11, -4, 3, 10, -12, -5, 2
.word -3, 4, 11, -11, -4, 3, 10, -12
.word -5, 2, 9, -13, -6, 1, 8, -14
.word 10, -12, -5, 2, 9, -13, -6, 1
.word 8, -14, -7, 0, 7, 14, -8, -1

matmul_b:
.word -10, 1, 12, -8, 3, 14, -6, 5
.word -15, -4, 7, -13, -2, 9, -11, 0
.word 9, -11, 0, 11, -9, 2, 13, -7
.word 4, 15, -5, 6, -14, -3, 8, -12
.word -3, 8, -12, -1, 10, -10, 1, 12
.word -8, 3, 14, -6, 5, -15, -4, 7
.word -15, -4, 7, -13, -2, 9, -11, 0
.word 11, -9, 2, 13, -7, 4, 15, -5
.word 4, 15, -5, 6, -14, -3, 8, -12
.word -1, 10, -10, 1, 12, -8, 3, 14
.word -8, 3, 14, -6, 5, -15, -4, 7
.word -13, -2, 9, -11, 0, 11, -9, 2
.word 11, -9, 2, 13, -7, 4, 15, -5
.word 6, -14, -3, 8, -12, -1, 10, -10
.word -1, 10, -10, 1, 12, -8, 3, 14
.word -6, 5, -15, -4, 7, -13, -2, 9
.word -13, -2, 9, -11, 0, 11, -9, 2
.word 13, -7, 4, 15, -5, 6, -14, -3
.word 6, -14, -3, 8, -12, -1, 10, -10
.word 1, 12, -8, 3, 14, -6, 5, -15
.word -6, 5, -15, -4, 7, -13, -2, 9
.word -11, 0, 11, -9, 2, 13, -7, 4
.word 13, -7, 4, 15, -5, 6, -14, -3
.word 8, -12, -1, 10, -10, 1, 12, -8
.word 1, 12, -8, 3, 14, -6, 5, -15
.word -4, 7, -13, -2, 9, -11, 0, 11
.word -11, 0, 11, -9, 2, 13, -7, 4
.word 15, -5, 6, -14, -3, 8, -12, -1
.word 8, -12, -1, 10, -10, 1, 12, -8
.word 3, 14, -6, 5, -15, -4, 7, -13
.word -4, 7, -13, -2, 9, -11, 0, 11
.word -9, 2, 13, -7, 4, 15, -5, 6

matmul_c:
.zero 1024

.text
    la a0, matmul_a
    la a1, matmul_b
    la a2, matmul_c
    li a3, 16
    li sp, 0x10000

    jal ra, matrix_multiply_vector

    # Compute Checksum
    la t0, matmul_c
    li t1, 256
    li a0, 0
L_chk_loop:
    lw t2, 0(t0)
    add a0, a0, t2
    addi t0, t0, 4
    addi t1, t1, -1
    bne t1, zero, L_chk_loop
    j L_prog_end

# Vector Matrix Multiplication Kernel
# a0 = ptr A, a1 = ptr B, a2 = ptr C, a3 = N (16)
matrix_multiply_vector:
    # 1. Zero out C (256 words)
    vsetvli t0, zero, e32, m1, ta, ma
    vxor.vv v1, v1, v1
    mv t1, a2
    li t2, 64               # 64 vectors * 4 words = 256 words
L_zero_loop:
    vse32.v v1, (t1)
    addi t1, t1, 16
    addi t2, t2, -1
    bne t2, zero, L_zero_loop

    # 2. Main multiplication loops: i-k-j
    li s0, 0                # i = 0
    mv s1, a0               # ptr_A_row = A
    mv s2, a2               # ptr_C_row = C

L_i_loop:
    li s3, 0                # k = 0
    mv s4, a1               # ptr_B_row = B

L_k_loop:
    lw s5, 0(s1)            # s5 = A[i*N + k]
    addi s1, s1, 4

    mv t3, s2               # ptr_C_curr = ptr_C_row
    mv t4, s4               # ptr_B_curr = ptr_B_row
    mv t5, a3               # remaining j = 16

L_j_vec_loop:
    vsetvli t6, t5, e32, m1, ta, ma
    vle32.v v2, (t3)        # load C[i, j..j+3]
    vle32.v v3, (t4)        # load B[k, j..j+3]
    vmacc.vx v2, s5, v3     # v2 += A[i, k] * v3
    vse32.v v2, (t3)        # store C[i, j..j+3]
    slli t0, t6, 2
    sub t5, t5, t6
    add t3, t3, t0
    add t4, t4, t0
    bne t5, zero, L_j_vec_loop

    addi s4, s4, 64         # advance ptr_B_row by 16 words (64 bytes)
    addi s3, s3, 1          # k++
    bne s3, a3, L_k_loop

    addi s2, s2, 64         # advance ptr_C_row by 16 words (64 bytes)
    addi s0, s0, 1          # i++
    bne s0, a3, L_i_loop

    ret

L_prog_end:

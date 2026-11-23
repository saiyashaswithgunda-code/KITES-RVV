.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    # 1D FIR Filter / Sliding Window Convolution:
    # Y[n] = 2 * X[n] - 1 * X[n+1] + 3 * X[n+2] for n = 0..5 (6 output samples, SEW=32)

    li a0, 0x80001000 # Address of Input Array X (8 elements)
    li a1, 0x80001040 # Address of Output Array Y (6 elements)

    # Initialize Input Array X (8 elements: 10, -5, 8, 2, -4, 7, 1, -3)
    li t0, 10
    sw t0, 0(a0)
    li t0, -5
    sw t0, 4(a0)
    li t0, 8
    sw t0, 8(a0)
    li t0, 2
    sw t0, 12(a0)
    li t0, -4
    sw t0, 16(a0)
    li t0, 7
    sw t0, 20(a0)
    li t0, 1
    sw t0, 24(a0)
    li t0, -3
    sw t0, 28(a0)

    # Filter coefficients: c0 = 2, c1 = -1, c2 = 3
    li t1, 2
    li t2, -1
    li t3, 3

    mv t4, a0 # input base pointer
    mv t5, a1 # output base pointer
    li s0, 6  # remaining output samples

fir_loop:
    vsetvli t0, s0, e32, m1, tu, mu

    # Load 3 overlapping windows for current batch
    vle32.v v1, (t4)     # X[n]
    addi s1, t4, 4
    vle32.v v2, (s1)     # X[n+1]
    addi s1, t4, 8
    vle32.v v3, (s1)     # X[n+2]

    # Multiply-accumulate
    vmul.vx v4, v1, t1   # 2 * X[n]
    vmul.vx v5, v2, t2   # -1 * X[n+1]
    vmul.vx v6, v3, t3   # 3 * X[n+2]
    vadd.vv v7, v4, v5
    vadd.vv v7, v7, v6

    # Store computed output samples
    vse32.v v7, (t5)

    slli s2, t0, 2
    add t4, t4, s2
    add t5, t5, s2
    sub s0, s0, t0
    bnez s0, fir_loop

test_done:
    ebreak

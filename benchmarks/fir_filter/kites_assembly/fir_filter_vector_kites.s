# FIR Filter - Vector RV32IMV (KITES)
# Expected checksum in a0 (x10): -8806 (0xFFFFDD9A)

.data
fir_inputs:
.word -26, -13, 0, 13, 26, -28, -15, -2
.word 11, 24, -30, -17, -4, 9, 22, -32
.word -19, -6, 7, 20, 33, -21, -8, 5
.word 18, 31, -23, -10, 3, 16, 29, -25
.word -12, 1, 14, 27, -27, -14, -1, 12
.word 25, -29, -16, -3, 10, 23, -31, -18
.word -5, 8, 21, -33, -20, -7, 6, 19
.word 32, -22, -9, 4, 17, 30, -24, -11
.word 2, 15, 28, -26, -13, 0, 13, 26
.word -28, -15, -2, 11, 24, -30, -17, -4
.word 9, 22, -32, -19, -6, 7, 20, 33
.word -21, -8, 5, 18, 31, -23, -10, 3
.word 16, 29, -25, -12, 1, 14, 27, -27
.word -14, -1, 12, 25, -29, -16, -3, 10
.word 23, -31, -18, -5, 8, 21, -33, -20
.word -7, 6, 19, 32, -22, -9, 4, 17
.word 30, -24, -11, 2, 15, 28, -26, -13
.word 0, 13, 26, -28, -15, -2, 11, 24
.word -30, -17, -4, 9, 22, -32, -19, -6
.word 7, 20, 33, -21, -8, 5, 18, 31
.word -23, -10, 3, 16, 29, -25, -12, 1
.word 14, 27, -27, -14, -1, 12, 25, -29
.word -16, -3, 10, 23, -31, -18, -5, 8
.word 21, -33, -20, -7, 6, 19, 32, -22
.word -9, 4, 17, 30, -24, -11, 2, 15
.word 28, -26, -13, 0, 13, 26, -28, -15
.word -2, 11, 24, -30, -17, -4, 9, 22
.word -32, -19, -6, 7, 20, 33, -21, -8
.word 5, 18, 31, -23, -10, 3, 16, 29
.word -25, -12, 1, 14, 27, -27, -14, -1
.word 12, 25, -29, -16, -3, 10, 23, -31
.word -18, -5, 8, 21, -33, -20, -7, 6

fir_taps:
.word -2, -3, 1, 8, 18, 28, 35, 38
.word 38, 35, 28, 18, 8, 1, -3, -2

fir_outputs:
.zero 1024

.text
    la a0, fir_inputs
    la a1, fir_taps
    la a2, fir_outputs
    li a3, 256              # num_samples
    li a4, 16               # num_taps
    li sp, 0x10000

    jal ra, fir_filter_vector

    # Compute Checksum
    la t0, fir_outputs
    li t1, 256
    li a0, 0
L_chk_loop:
    lw t2, 0(t0)
    add a0, a0, t2
    addi t0, t0, 4
    addi t1, t1, -1
    bne t1, zero, L_chk_loop
    j L_prog_end

# Vector FIR Filter Kernel
# a0 = inputs, a1 = taps, a2 = outputs, a3 = num_samples (256), a4 = num_taps (16)
fir_filter_vector:
    # 1. Zero out outputs (256 words)
    vsetvli t0, zero, e32, m1, ta, ma
    vxor.vv v1, v1, v1
    mv t1, a2
    li t2, 64               # 64 vectors * 4 words = 256 words
L_zero_loop:
    vse32.v v1, (t1)
    addi t1, t1, 16
    addi t2, t2, -1
    bne t2, zero, L_zero_loop

    # 2. Tap accumulation loop (k = 0 .. num_taps-1)
    li s0, 0                # k = 0
    mv s1, a1               # ptr_taps = taps

L_tap_loop:
    lw s2, 0(s1)            # s2 = taps[k]
    addi s1, s1, 4

    # Remaining length for this tap: L = num_samples - k
    sub s3, a3, s0

    # ptr_y = outputs + k * 4
    slli s4, s0, 2
    add s4, a2, s4

    # ptr_x = inputs
    mv s5, a0

L_sample_vec_loop:
    vsetvli t3, s3, e32, m1, ta, ma
    vle32.v v2, (s4)        # load slice of y
    vle32.v v3, (s5)        # load slice of input
    vmacc.vx v2, s2, v3     # v2 += taps[k] * input
    vse32.v v2, (s4)        # store slice of y
    slli t4, t3, 2
    sub s3, s3, t3
    add s4, s4, t4
    add s5, s5, t4
    bne s3, zero, L_sample_vec_loop

    addi s0, s0, 1          # k++
    bne s0, a4, L_tap_loop

    ret

L_prog_end:

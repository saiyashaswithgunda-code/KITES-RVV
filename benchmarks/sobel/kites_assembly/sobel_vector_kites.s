# Sobel Filter - Vector RV32IMV (KITES)
# Expected checksum in a0 (x10): 53140 (0x0000CF94)

.data
sobel_input_image:
.byte 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120
.byte 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30
.byte 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30
.byte 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30
.byte 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 200, 200, 200, 200, 200, 200, 200, 200
.byte 200, 200, 200, 200, 200, 200, 200, 200, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30, 30
.byte 30, 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30, 30
.byte 30, 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30, 30
.byte 30, 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120, 30
.byte 120, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
.byte 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 120

sobel_output_image:
.zero 1024

.text
    la a0, sobel_input_image
    la a1, sobel_output_image
    li a2, 32               # width
    li a3, 32               # height
    li sp, 0x10000          # scratch stack

    jal ra, sobel_filter_vector

    # Compute Checksum
    la t0, sobel_output_image
    li t1, 1024
    li a0, 0
L_chk_loop:
    lbu t2, 0(t0)
    add a0, a0, t2
    addi t0, t0, 1
    addi t1, t1, -1
    bne t1, zero, L_chk_loop
    j L_prog_end

# Vector Sobel Filter Kernel
# a0 = input, a1 = output, a2 = width (32), a3 = height (32)
# Stack frame layout at sp:
#   0..23:   row0 unpacked words (6 words)
#  24..47:   row1 unpacked words (6 words)
#  48..71:   row2 unpacked words (6 words)
#  72..87:   output mag words (4 words)
sobel_filter_vector:
    # 1. Zero out output image (1024 bytes) using vse8.v
    vsetvli t0, zero, e8, m1, ta, ma
    vxor.vv v1, v1, v1
    mv t1, a1
    li t2, 64               # 64 * 16 = 1024 bytes
L_zero_loop:
    vse8.v v1, (t1)
    addi t1, t1, 16
    addi t2, t2, -1
    bne t2, zero, L_zero_loop

    # Configure vector unit for 4 x 32-bit vector operations
    vsetvli t0, zero, e32, m1, ta, ma

    # 2. Iterate over interior rows y = 1 .. 30
    li s0, 1                # y = 1
L_row_loop:
    # In each row, process 8 chunks of 4 pixels at x = 1, 5, 9, 13, 17, 21, 25, 27
    li s1, 0                # chunk_idx = 0

L_chunk_loop:
    # Compute x_start based on chunk_idx
    # if chunk_idx == 7: x_start = 27, else x_start = 1 + chunk_idx * 4
    li t0, 7
    beq s1, t0, L_chunk_27
    slli s2, s1, 2
    addi s2, s2, 1          # s2 = x_start
    j L_have_x_start
L_chunk_27:
    li s2, 27               # s2 = 27
L_have_x_start:

    # Pointers to input image:
    # row0_ptr = input + (y-1)*32 + (x_start - 1)
    # row1_ptr = input + y*32 + (x_start - 1)
    # row2_ptr = input + (y+1)*32 + (x_start - 1)
    addi t0, s0, -1
    slli t0, t0, 5          # (y-1)*32
    add t0, t0, s2
    addi t0, t0, -1         # + x_start - 1
    add t1, a0, t0          # t1 = row0_ptr
    addi t2, t1, 32         # t2 = row1_ptr
    addi t3, t2, 32         # t3 = row2_ptr

    # Unpack 6 bytes of row0 to sp + 0
    lbu t4, 0(t1)
    sw t4, 0(sp)
    lbu t4, 1(t1)
    sw t4, 4(sp)
    lbu t4, 2(t1)
    sw t4, 8(sp)
    lbu t4, 3(t1)
    sw t4, 12(sp)
    lbu t4, 4(t1)
    sw t4, 16(sp)
    lbu t4, 5(t1)
    sw t4, 20(sp)

    # Unpack 6 bytes of row1 to sp + 24
    lbu t4, 0(t2)
    sw t4, 24(sp)
    lbu t4, 1(t2)
    sw t4, 28(sp)
    lbu t4, 2(t2)
    sw t4, 32(sp)
    lbu t4, 3(t2)
    sw t4, 36(sp)
    lbu t4, 4(t2)
    sw t4, 40(sp)
    lbu t4, 5(t2)
    sw t4, 44(sp)

    # Unpack 6 bytes of row2 to sp + 48
    lbu t4, 0(t3)
    sw t4, 48(sp)
    lbu t4, 1(t3)
    sw t4, 52(sp)
    lbu t4, 2(t3)
    sw t4, 56(sp)
    lbu t4, 3(t3)
    sw t4, 60(sp)
    lbu t4, 4(t3)
    sw t4, 64(sp)
    lbu t4, 5(t3)
    sw t4, 68(sp)

    # Load 32-bit vector registers (4 elements each):
    # v1 = p00 (sp + 0), v2 = p01 (sp + 4), v3 = p02 (sp + 8)
    addi t0, sp, 0
    vle32.v v1, (t0)
    addi t0, sp, 4
    vle32.v v2, (t0)
    addi t0, sp, 8
    vle32.v v3, (t0)

    # v4 = p10 (sp + 24), v5 = p12 (sp + 32)
    addi t0, sp, 24
    vle32.v v4, (t0)
    addi t0, sp, 32
    vle32.v v5, (t0)

    # v6 = p20 (sp + 48), v7 = p21 (sp + 52), v8 = p22 (sp + 56)
    addi t0, sp, 48
    vle32.v v6, (t0)
    addi t0, sp, 52
    vle32.v v7, (t0)
    addi t0, sp, 56
    vle32.v v8, (t0)

    # --- Vector Sobel Math ---
    # gx = (p02 - p00) + 2 * (p12 - p10) + (p22 - p20)
    # v9 = gx
    vsub.vv v9, v3, v1      # (p02 - p00)
    vsub.vv v10, v5, v4     # (p12 - p10)
    vsll.vi v10, v10, 1     # 2 * (p12 - p10)
    vadd.vv v9, v9, v10
    vsub.vv v10, v8, v6     # (p22 - p20)
    vadd.vv v9, v9, v10     # v9 = gx

    # gy = (p00 - p20) + 2 * (p01 - p21) + (p02 - p22)
    # v11 = gy
    vsub.vv v11, v1, v6     # (p00 - p20)
    vsub.vv v10, v2, v7     # (p01 - p21)
    vsll.vi v10, v10, 1     # 2 * (p01 - p21)
    vadd.vv v11, v11, v10
    vsub.vv v10, v3, v8     # (p02 - p22)
    vadd.vv v11, v11, v10   # v11 = gy

    # abs(gx): v12 = max(gx, -gx)
    vrsub.vi v10, v9, 0     # -gx
    vmax.vv v12, v9, v10    # abs(gx)

    # abs(gy): v13 = max(gy, -gy)
    vrsub.vi v10, v11, 0    # -gy
    vmax.vv v13, v11, v10   # abs(gy)

    # mag = abs(gx) + abs(gy)
    vadd.vv v12, v12, v13

    # clamp to 255: v12 = minu(v12, 255)
    li t0, 255
    vminu.vx v12, v12, t0

    # Store 4 result words to sp + 72
    addi t0, sp, 72
    vse32.v v12, (t0)

    # Pack 4 words to 4 bytes at output + y*32 + x_start
    slli t0, s0, 5
    add t0, t0, s2
    add t1, a1, t0          # ptr_out
    lw t4, 72(sp)
    sb t4, 0(t1)
    lw t4, 76(sp)
    sb t4, 1(t1)
    lw t4, 80(sp)
    sb t4, 2(t1)
    lw t4, 84(sp)
    sb t4, 3(t1)

    addi s1, s1, 1          # chunk_idx++
    li t0, 8
    bne s1, t0, L_chunk_loop

    addi s0, s0, 1          # y++
    li t0, 31
    bne s0, t0, L_row_loop

    ret

L_prog_end:

# Matrix Multiplication - Scalar RV32IM (KITES)
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

    jal ra, matrix_multiply

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

matrix_multiply:
	mul	a5,a3,a3
	addi	sp,sp,-48
	sw	s0,44(sp)
	sw	s5,24(sp)
	sw	s6,20(sp)
	mv	s0,a3
	mv	s6,a0
	mv	s5,a2
	beq	a5,zero,L2
	slli	a5,a5,2
	add	a4,a2,a5
	mv	a5,a2
L3:
	sw	zero,0(a5)
	addi	a5,a5,4
	bne	a5,a4,L3
L2:
	beq	s0,zero,L1
	sw	s7,16(sp)
	addi	s7,s0,-3
	sw	s1,40(sp)
	srli	s7,s7,1
	slli	s1,s0,2
	neg	a2,s0
	sw	s2,36(sp)
	sw	s3,32(sp)
	sw	s4,28(sp)
	sw	s8,12(sp)
	sw	s9,8(sp)
	addi	s7,s7,1
	add	t3,s5,s1
	slli	a2,a2,3
	mv	s4,s6
	mv	a0,s5
	li	s3,0
	li	s2,0
	li	s8,2
L5:
	li	a3,0
	bleu	s0,s8,L13
	slli	a3,s7,1
	mv	t4,s1
	mv	t0,s4
	li	t1,0
	li	t2,0
L6:
	lw	t6,0(t0)
	lw	t5,4(t0)
	sub	a6,a1,t1
	mv	a4,a0
L10:
	add	a7,a6,t1
	lw	a5,0(a6)
	add	a7,a7,t4
	lw	a7,0(a7)
	mul	a5,t6,a5
	lw	s9,0(a4)
	addi	a4,a4,4
	addi	a6,a6,4
	mul	a7,t5,a7
	add	a5,a5,s9
	add	a5,a5,a7
	sw	a5,-4(a4)
	bne	a4,t3,L10
	addi	t2,t2,2
	addi	t0,t0,8
	add	t1,t1,a2
	sub	t4,t4,a2
	bne	t2,a3,L6
L13:
	mul	t4,s0,a3
	slli	t5,s3,2
	add	t6,s6,t5
	add	t5,s5,t5
L8:
	slli	a5,a3,2
	add	a5,t6,a5
	slli	a7,t4,2
	lw	t1,0(a5)
	add	a7,a1,a7
	mv	a5,t5
L7:
	lw	a6,0(a7)
	lw	a4,0(a5)
	addi	a5,a5,4
	mul	a6,t1,a6
	addi	a7,a7,4
	add	a4,a4,a6
	sw	a4,-4(a5)
	bne	a5,t3,L7
	addi	a3,a3,1
	add	t4,t4,s0
	bgtu	s0,a3,L8
	addi	s2,s2,1
	add	a0,a0,s1
	add	s3,s3,s0
	add	t3,t3,s1
	add	s4,s4,s1
	bne	s0,s2,L5
	lw	s1,40(sp)
	lw	s2,36(sp)
	lw	s3,32(sp)
	lw	s4,28(sp)
	lw	s7,16(sp)
	lw	s8,12(sp)
	lw	s9,8(sp)
L1:
	lw	s0,44(sp)
	lw	s5,24(sp)
	lw	s6,20(sp)
	addi	sp,sp,48
	jr	ra

L_prog_end:

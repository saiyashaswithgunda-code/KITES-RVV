# FIR Filter - Scalar RV32IM (KITES)
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
    li a3, 256
    li a4, 16
    li sp, 0x10000

    jal ra, fir_filter

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

fir_filter:
	beq	a3,zero,L1
	beq	a4,zero,L11
	mv	t5,a3
	bleu	a3,a4,L4
	mv	t5,a4
L4:
	mv	t2,a2
	mv	t0,a0
	li	t6,0
	addi	t4,a0,-4
L6:
	addi	t6,t6,1
	mv	a7,a1
	mv	a5,t0
	li	t1,0
L5:
	lw	a6,0(a7)
	lw	t3,0(a5)
	addi	a5,a5,-4
	addi	a7,a7,4
	mul	a6,a6,t3
	add	t1,t1,a6
	bne	t4,a5,L5
	sw	t1,0(t2)
	addi	t0,t0,4
	addi	t2,t2,4
	bgtu	t5,t6,L6
	bleu	a3,a4,L20
L3:
	slli	a5,t5,2
	add	t1,a2,a5
	add	t3,a0,a5
L10:
	addi	t5,t5,1
	bne	a4,zero,L21
	sw	zero,0(t1)
	addi	t3,t3,4
	addi	t1,t1,4
	bgtu	a3,t5,L10
L1:
	ret
L11:
	li	t5,0
	j	L3
L21:
	slli	a4,a4,2
	add	a7,a1,a4
L9:
	mv	a2,t3
	mv	a5,a1
	li	a0,0
L8:
	lw	a4,0(a2)
	lw	a6,0(a5)
	addi	a5,a5,4
	addi	a2,a2,-4
	mul	a4,a4,a6
	add	a0,a0,a4
	bne	a5,a7,L8
	sw	a0,0(t1)
	bleu	a3,t5,L1
	addi	t1,t1,4
	addi	t3,t3,4
	addi	t5,t5,1
	j	L9
L20:
	ret

L_prog_end:

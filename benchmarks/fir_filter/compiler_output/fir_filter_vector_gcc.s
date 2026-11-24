	.file	"fir_filter.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_f2p2_d2p2_v1p0_zicsr2p0_zmmul1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	fir_filter
	.type	fir_filter, @function
fir_filter:
	beq	a3,zero,.L34
	addi	sp,sp,-32
	sw	s0,28(sp)
	sw	s1,24(sp)
	mv	s0,a2
	mv	s1,a0
	beq	a4,zero,.L19
	sw	s2,20(sp)
	sw	s3,16(sp)
	sw	s4,12(sp)
	sw	s5,8(sp)
	mv	t5,a3
	bleu	a3,a4,.L4
	mv	t5,a4
.L4:
	vsetvli	t2,zero,e32,m1,ta,ma
	csrr	t3,vlenb
	srli	a2,t3,2
	vid.v	v5
	addi	s3,a2,-1
	vrsub.vx	v5,v5,s3
	neg	t0,a2
	addi	s4,t3,-4
	vand.vx	v5,v5,s3
	addi	t0,t0,1
	sub	a0,s1,s4
	mv	s2,s0
	li	t6,0
.L9:
	mv	a5,t6
	addi	t6,t6,1
	bltu	a5,s3,.L20
	vmv.v.i	v1,0
	mv	t1,a0
	mv	a7,a1
	li	a5,0
.L6:
	vl1re32.v	v4,0(t1)
	vl1re32.v	v3,0(a7)
	add	a5,a5,a2
	sub	t1,t1,t3
	add	a7,a7,t3
	vrgather.vv	v2,v4,v5
	vmacc.vv	v1,v3,v2
	bleu	a5,t0,.L6
	vmv.s.x	v2,zero
	vredsum.vs	v1,v1,v2
	vmv.x.s	t4,v1
	beq	t6,a5,.L7
.L5:
	slli	a6,a5,2
	add	a7,a0,s4
	add	t1,a1,a6
	sub	a7,a7,a6
.L8:
	lw	a6,0(t1)
	lw	s5,0(a7)
	addi	a5,a5,1
	addi	t1,t1,4
	mul	a6,a6,s5
	addi	a7,a7,-4
	add	t4,t4,a6
	bgtu	t6,a5,.L8
.L7:
	sw	t4,0(s2)
	addi	a0,a0,4
	addi	s2,s2,4
	addi	t0,t0,1
	bgtu	t5,t6,.L9
	lw	s2,20(sp)
	lw	s3,16(sp)
	lw	s4,12(sp)
	lw	s5,8(sp)
	bleu	a3,a4,.L32
.L3:
	slli	a5,t5,2
	add	s0,s0,a5
	add	s1,s1,a5
.L18:
	addi	t5,t5,1
	bne	a4,zero,.L43
	sw	zero,0(s0)
	addi	s1,s1,4
	addi	s0,s0,4
	bgtu	a3,t5,.L18
.L32:
	lw	s0,28(sp)
	lw	s1,24(sp)
	addi	sp,sp,32
	jr	ra
.L19:
	li	t5,0
	j	.L3
.L20:
	li	a5,0
	li	t4,0
	j	.L5
.L43:
	vsetvli	t3,zero,e32,m1,ta,ma
	csrr	t6,vlenb
	srli	t4,t6,2
	vid.v	v5
	sw	s2,20(sp)
	addi	s2,t4,-1
	vrsub.vx	v5,v5,s2
	sw	s3,16(sp)
	sw	s4,12(sp)
	neg	t0,t6
	addi	s3,a4,-1
	vand.vx	v5,v5,s2
	sub	t2,a4,t4
	addi	s4,t0,4
	bltu	s3,s2,.L21
.L44:
	vmv.v.i	v1,0
	add	a6,s1,s4
	mv	a0,a1
	li	a5,0
.L13:
	vl1re32.v	v4,0(a6)
	vl1re32.v	v2,0(a0)
	add	a5,a5,t4
	add	a6,a6,t0
	add	a0,a0,t6
	vrgather.vv	v3,v4,v5
	vmacc.vv	v1,v3,v2
	bgeu	t2,a5,.L13
	vmv.s.x	v2,zero
	vredsum.vs	v1,v1,v2
	vmv.x.s	a7,v1
	beq	a4,a5,.L31
.L12:
	slli	a0,a5,2
	add	a6,a1,a0
	sub	a0,s1,a0
.L15:
	lw	a2,0(a6)
	lw	t1,0(a0)
	addi	a5,a5,1
	addi	a6,a6,4
	mul	a2,a2,t1
	addi	a0,a0,-4
	add	a7,a7,a2
	bgtu	a4,a5,.L15
.L31:
	sw	a7,0(s0)
	bleu	a3,t5,.L39
	addi	s0,s0,4
	addi	s1,s1,4
	addi	t5,t5,1
	bgeu	s3,s2,.L44
.L21:
	li	a5,0
	li	a7,0
	j	.L12
.L39:
	lw	s0,28(sp)
	lw	s2,20(sp)
	lw	s3,16(sp)
	lw	s4,12(sp)
	lw	s1,24(sp)
	addi	sp,sp,32
	jr	ra
.L34:
	ret
	.size	fir_filter, .-fir_filter
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits

	.file	"sobel.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_f2p2_d2p2_v1p0_zicsr2p0_zmmul1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	sobel_filter
	.type	sobel_filter, @function
sobel_filter:
	mul	a5,a2,a3
	addi	sp,sp,-32
	sw	s2,20(sp)
	beq	a5,zero,.L2
	vsetvli	a4,zero,e8,m1,ta,ma
	vmv.v.i	v1,0
	mv	a4,a1
.L3:
	vsetvli	a6,a5,e8,m1,ta,ma
	vse8.v	v1,0(a4)
	sub	a5,a5,a6
	add	a4,a4,a6
	bne	a5,zero,.L3
.L2:
	addi	s2,a3,-1
	li	t2,1
	bleu	s2,t2,.L13
	addi	a5,a2,-1
	bleu	a5,t2,.L13
	vsetvli	a5,zero,e16,m1,ta,ma
	li	a5,255
	vmv.v.i	v5,2
	vmv.v.x	v13,a5
	sw	s0,28(sp)
	sw	s3,16(sp)
	addi	s0,a0,2
	slli	s3,a2,1
	add	a1,a1,t2
	sw	s1,24(sp)
	sw	s4,12(sp)
	add	t0,a1,a2
	addi	s4,s3,-2
	add	t6,s0,a2
	addi	s3,s3,-1
	addi	s1,a2,-2
.L6:
	mv	a1,s0
	addi	t5,s0,-2
	mv	t4,t6
	mv	s0,t6
	add	t6,t6,a2
	addi	t2,t2,1
	add	t3,a1,s1
	mv	t1,t6
	add	a7,a1,s4
	addi	a6,a1,-1
	add	a0,a1,s3
	mv	a3,s1
	mv	a4,t0
.L5:
	vsetvli	a5,a3,e8,mf2,ta,ma
	vle8.v	v1,0(t5)
	vle8.v	v4,0(a1)
	vle8.v	v3,0(t1)
	vle8.v	v2,0(a7)
	vle8.v	v11,0(t4)
	vle8.v	v10,0(t3)
	vle8.v	v7,0(a6)
	vwaddu.vv	v8,v1,v4
	vle8.v	v6,0(a0)
	vwaddu.vv	v12,v4,v3
	vwsubu.wv	v9,v8,v2
	sub	a3,a3,a5
	vwsubu.wv	v8,v12,v1
	vwsubu.wv	v4,v9,v3
	vwsubu.vv	v1,v11,v10
	vwsubu.wv	v3,v8,v2
	vwsubu.vv	v2,v7,v6
	vsetvli	zero,zero,e16,m1,ta,ma
	vmadd.vv	v1,v5,v3
	vmadd.vv	v2,v5,v4
	add	t4,t4,a5
	add	t3,t3,a5
	vrsub.vi	v4,v1,0
	vrsub.vi	v3,v2,0
	add	a7,a7,a5
	vmax.vv	v1,v1,v4
	vmax.vv	v2,v2,v3
	add	t5,t5,a5
	add	a1,a1,a5
	vadd.vv	v1,v1,v2
	add	t1,t1,a5
	add	a6,a6,a5
	vminu.vv	v1,v1,v13
	vsetvli	zero,zero,e8,mf2,ta,ma
	add	a0,a0,a5
	vnsrl.wi	v1,v1,0
	vse8.v	v1,0(a4)
	add	a4,a4,a5
	bne	a3,zero,.L5
	add	t0,t0,a2
	bne	t2,s2,.L6
	lw	s0,28(sp)
	lw	s1,24(sp)
	lw	s3,16(sp)
	lw	s4,12(sp)
.L13:
	lw	s2,20(sp)
	addi	sp,sp,32
	jr	ra
	.size	sobel_filter, .-sobel_filter
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits

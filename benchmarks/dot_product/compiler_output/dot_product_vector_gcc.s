	.file	"dot_product.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_f2p2_d2p2_v1p0_zicsr2p0_zmmul1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	dot_product
	.type	dot_product, @function
dot_product:
	beq	a2,zero,.L4
	vsetvli	a5,zero,e32,m1,ta,ma
	vmv.v.i	v1,0
.L3:
	vsetvli	a5,a2,e32,m1,tu,ma
	vle32.v	v2,0(a0)
	vle32.v	v3,0(a1)
	slli	a4,a5,2
	sub	a2,a2,a5
	add	a0,a0,a4
	add	a1,a1,a4
	vmacc.vv	v1,v3,v2
	bne	a2,zero,.L3
	vsetvli	a5,zero,e32,m1,ta,ma
	vmv.s.x	v2,zero
	vredsum.vs	v1,v1,v2
	vmv.x.s	a0,v1
	ret
.L4:
	li	a0,0
	ret
	.size	dot_product, .-dot_product
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits

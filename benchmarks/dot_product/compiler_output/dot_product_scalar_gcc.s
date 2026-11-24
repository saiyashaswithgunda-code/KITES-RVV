	.file	"dot_product.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_zmmul1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	dot_product
	.type	dot_product, @function
dot_product:
	beq	a2,zero,.L4
	slli	a2,a2,2
	mv	a5,a0
	add	a2,a0,a2
	li	a0,0
.L3:
	lw	a4,0(a5)
	lw	a3,0(a1)
	addi	a5,a5,4
	addi	a1,a1,4
	mul	a4,a4,a3
	add	a0,a0,a4
	bne	a2,a5,.L3
	ret
.L4:
	li	a0,0
	ret
	.size	dot_product, .-dot_product
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits

.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001010

    # Pattern for v1: 0, 1, 0, 1
    li t0, 0
    sw t0, 0(a0)
    li t0, 1
    sw t0, 4(a0)
    li t0, 0
    sw t0, 8(a0)
    li t0, 1
    sw t0, 12(a0)

    # Pattern for v2: 0, 0, 1, 1
    li t0, 0
    sw t0, 0(a1)
    li t0, 0
    sw t0, 4(a1)
    li t0, 1
    sw t0, 8(a1)
    li t0, 1
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    # Generate masks in v10 and v11
    vmseq.vi v10, v1, 1 # bits: 0b1010 (10)
    vmseq.vi v11, v2, 1 # bits: 0b1100 (12)

    # Mask logical operations
    vmand.mm v12, v10, v11
    vmandn.mm v13, v10, v11
    vmor.mm v14, v10, v11
    vmorn.mm v15, v10, v11
    vmxor.mm v16, v10, v11

    # Mask prefix operations
    vmsbf.m v17, v10
    vmsof.m v18, v10
    vmsif.m v19, v10

test_done:
    ebreak

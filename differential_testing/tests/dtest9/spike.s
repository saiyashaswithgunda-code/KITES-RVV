.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001010

    # Store 4 32-bit words for v1
    li t0, 10
    sw t0, 0(a0)
    li t0, -20
    sw t0, 4(a0)
    li t0, 30
    sw t0, 8(a0)
    li t0, -40
    sw t0, 12(a0)

    # Store 4 32-bit words for v2
    li t0, 10
    sw t0, 0(a1)
    li t0, 5
    sw t0, 4(a1)
    li t0, 20
    sw t0, 8(a1)
    li t0, -40
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    vmseq.vv v3, v1, v2
    vmsne.vv v4, v1, v2
    vmslt.vv v5, v1, v2
    vmsltu.vv v6, v1, v2
    vmsle.vv v7, v1, v2
    vmsleu.vv v8, v1, v2

test_done:
    ebreak

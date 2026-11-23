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
    li t0, -5
    sw t0, 4(a0)
    li t0, 20
    sw t0, 8(a0)
    li t0, -15
    sw t0, 12(a0)

    # Store scalar accumulator for v2
    li t0, 100
    sw t0, 0(a1)
    li t0, 0
    sw t0, 4(a1)
    sw t0, 8(a1)
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1, tail undisturbed
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu

    vle32.v v1, (a0)
    vle32.v v2, (a1)

    # Generate mask in v0: active where v1 > 0
    vmsgt.vi v0, v1, 0

    # Masked reductions
    vredsum.vs v3, v1, v2, v0.t
    vredmax.vs v4, v1, v2, v0.t
    vredmin.vs v5, v1, v2, v0.t
    vredand.vs v6, v1, v2, v0.t
    vredor.vs v7, v1, v2, v0.t
    vredxor.vs v8, v1, v2, v0.t

test_done:
    ebreak

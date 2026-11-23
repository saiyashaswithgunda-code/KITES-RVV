.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    li a0, 0x80001000
    li a1, 0x80001020

    # Source scalar at a0: 999
    li t0, 999
    sw t0, 0(a0)

    # Pre-fill destination memory at a1 with [1, 2, 3, 4]
    li t0, 1
    sw t0, 0(a1)
    li t0, 2
    sw t0, 4(a1)
    li t0, 3
    sw t0, 8(a1)
    li t0, 4
    sw t0, 12(a1)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    # Zero-stride load: broadcasts 999 across all elements of v1
    vlse32.v v1, (a0), x0

    # Load pre-filled destination into v2
    vle32.v v2, (a1)

    # Generate mask in v0: active where v2 != 2 (indices 0, 2, 3 active; index 1 inactive)
    vmsne.vi v0, v2, 2

    # Masked strided store: only stores elements 0, 2, and 3; leaves index 1 untouched
    li t3, 4
    vsse32.v v1, (a1), t3, v0.t

test_done:
    ebreak

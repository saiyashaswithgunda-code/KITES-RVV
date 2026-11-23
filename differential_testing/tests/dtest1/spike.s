
.section .text
.globl _start

_start:
    # Enable vector state
    li      t2, 0x600
    csrs    mstatus, t2

    # Initialize input memory

    li a0, 0x80001000

    li      t1, 1
    sw      t1, 0(a0)
    li      t1, 2
    sw      t1, 4(a0)
    li      t1, 3
    sw      t1, 8(a0)
    li      t1, 4
    sw      t1, 12(a0)

    # Configure vector: 4 elements, 32-bit, LMUL=1
    li      t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    # Load input
    vle32.v v1, (a0)

    # Double each element
    vadd.vv v2, v1, v1

    # Store result at 0x120
   li a1, 0x80001020
   vse32.v v2, (a1)

test_done:
    ebreak

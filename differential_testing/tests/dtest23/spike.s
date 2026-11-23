.section .text
.globl _start
_start:
    li t2, 0x600
    csrs mstatus, t2

    # Sum of Array Elements using Vector Reduction (11 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001040 # Address of output scalar sum

    # Initialize Array A (11 elements)
    li t0, 12
    sw t0, 0(a0)
    li t0, -45
    sw t0, 4(a0)
    li t0, 78
    sw t0, 8(a0)
    li t0, -23
    sw t0, 12(a0)
    li t0, 56
    sw t0, 16(a0)
    li t0, -89
    sw t0, 20(a0)
    li t0, 90
    sw t0, 24(a0)
    li t0, -12
    sw t0, 28(a0)
    li t0, 34
    sw t0, 32(a0)
    li t0, -67
    sw t0, 36(a0)
    li t0, 10
    sw t0, 40(a0)

    # Initialize scalar accumulator v10 to 0
    li t1, 4
    vsetvli t0, t1, e32, m1, tu, mu
    vxor.vv v10, v10, v10

    mv t3, a0
    li s0, 11 # remaining elements

sum_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vredsum.vs v10, v1, v10

    slli s1, t0, 2
    add t3, t3, s1
    sub s0, s0, t0
    bnez s0, sum_loop

    # Store final scalar sum from v10[0]
    vse32.v v10, (a1)

test_done:
    ebreak

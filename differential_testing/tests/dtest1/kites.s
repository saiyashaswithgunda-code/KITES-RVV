
    li a0, 2147487744

    li t1, 1
    sw t1, 0(a0)
    li t1, 2
    sw t1, 4(a0)
    li t1, 3
    sw t1, 8(a0)
    li t1, 4
    sw t1, 12(a0)

    li t1, 4
    vsetvli t0, t1, e32, m1, ta, ma

    vle32.v v1, (a0)
    vadd.vv v2, v1, v1

    li a1, 2147487776
    vse32.v v2, (a1)

test_done:
    ebreak

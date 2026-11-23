    # Initialize GP registers
    li a0, 0
    li a1, 0

    # 1. AVL = 0 (vl should be 0, no elements modified)
    li t1, 0
    vsetvli t0, t1, e32, m1, tu, mu
    vadd.vi v1, v1, 10

    # 2. AVL = 1 (vl should be 1, only element 0 modified)
    li t1, 1
    vsetvli t2, t1, e32, m1, tu, mu
    vadd.vi v2, v1, 7

    # 3. AVL = 100 with e32 (vl capped at VLMAX = 4)
    li t1, 100
    vsetvli t3, t1, e32, m1, tu, mu
    vadd.vi v3, v2, 5

    # 4. AVL = 50 with e16 (vl capped at VLMAX = 8)
    li t1, 50
    vsetvli t4, t1, e16, m1, tu, mu
    vadd.vi v4, v3, 3

    # 5. AVL = 50 with e64 (vl capped at VLMAX = 2)
    li t1, 50
    vsetvli t5, t1, e64, m1, tu, mu
    vadd.vi v5, v3, 1

test_done:
    ebreak

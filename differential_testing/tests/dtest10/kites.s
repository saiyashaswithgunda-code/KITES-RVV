    li a0, 0x80001000
    li a1, 0

    # Store 8 16-bit words for v1
    li t0, -10
    sh t0, 0(a0)
    li t0, 0
    sh t0, 2(a0)
    li t0, 5
    sh t0, 4(a0)
    li t0, 15
    sh t0, 6(a0)
    li t0, -30
    sh t0, 8(a0)
    li t0, 20
    sh t0, 10(a0)
    li t0, 100
    sh t0, 12(a0)
    li t0, -1
    sh t0, 14(a0)

    # Configure vector: 8 elements, 16-bit, LMUL=1
    li t1, 8
    vsetvli t0, t1, e16, m1, ta, ma

    vle16.v v1, (a0)

    li t3, 5
    li t4, -10

    vmseq.vx v2, v1, t3
    vmseq.vi v3, v1, 5
    vmsne.vx v4, v1, t3
    vmsne.vi v5, v1, 0
    vmslt.vx v6, v1, t3
    vmsltu.vx v7, v1, t3
    vmsle.vx v8, v1, t4
    vmsle.vi v9, v1, 5
    vmsleu.vx v10, v1, t3
    vmsleu.vi v11, v1, 5
    vmsgt.vx v12, v1, t4
    vmsgt.vi v13, v1, 0
    vmsgtu.vx v14, v1, t3
    vmsgtu.vi v15, v1, 5

test_done:
    ebreak

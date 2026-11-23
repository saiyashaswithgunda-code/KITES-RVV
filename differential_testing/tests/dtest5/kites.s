    li a0, 0x80001000
    li a1, 0x80001010

    # Store 2 64-bit words for v1 using 32-bit words
    # Word 0: 0x0123456789ABCDEF
    li t0, 0x89ABCDEF
    sw t0, 0(a0)
    li t0, 0x01234567
    sw t0, 4(a0)

    # Word 1: 0xFEDCBA9876543210
    li t0, 0x76543210
    sw t0, 8(a0)
    li t0, 0xFEDCBA98
    sw t0, 12(a0)

    # Store 2 64-bit words for v2
    # Word 0: 0xAAAAAAAAAAAAAAAA
    li t0, 0xAAAAAAAA
    sw t0, 0(a1)
    sw t0, 4(a1)

    # Word 1: 0x5555555555555555
    li t0, 0x55555555
    sw t0, 8(a1)
    sw t0, 12(a1)

    # Configure vector: 2 elements, 64-bit, LMUL=1
    li t1, 2
    vsetvli t0, t1, e64, m1, ta, ma

    vle64.v v1, (a0)
    vle64.v v2, (a1)

    vand.vv v3, v1, v2
    vor.vv v4, v1, v2
    vxor.vv v5, v1, v2
    vxor.vv v6, v1, v1

test_done:
    ebreak

    # Array Transformation: B[i] = max(((A[i] * 3) - 10) XOR pattern[i], 0) (7 elements, SEW=32)
    li a0, 0x80001000 # Address of Array A
    li a1, 0x80001020 # Address of Pattern Array
    li a2, 0x80001040 # Address of Output Array B

    # Initialize Array A (7 elements: 5, -8, 12, -2, 0, 15, -20)
    li t0, 5
    sw t0, 0(a0)
    li t0, -8
    sw t0, 4(a0)
    li t0, 12
    sw t0, 8(a0)
    li t0, -2
    sw t0, 12(a0)
    li t0, 0
    sw t0, 16(a0)
    li t0, 15
    sw t0, 20(a0)
    li t0, -20
    sw t0, 24(a0)

    # Initialize Pattern Array (7 elements)
    li t0, 0x00000007
    sw t0, 0(a1)
    li t0, 0x0000000F
    sw t0, 4(a1)
    li t0, 0x00000003
    sw t0, 8(a1)
    li t0, 0x00000001
    sw t0, 12(a1)
    li t0, 0x00000005
    sw t0, 16(a1)
    li t0, 0x00000002
    sw t0, 20(a1)
    li t0, 0x00000008
    sw t0, 24(a1)

    li t2, 3 # Scale factor = 3
    mv t3, a0
    mv t4, a1
    mv t5, a2
    li s0, 7 # remaining elements

transform_loop:
    vsetvli t0, s0, e32, m1, tu, mu
    vle32.v v1, (t3)
    vle32.v v2, (t4)

    # 1. Scale by 3
    vmul.vx v3, v1, t2

    # 2. Add bias -10 (simm5 in [-16, 15])
    vadd.vi v3, v3, -10

    # 3. Bitwise XOR with pattern
    vxor.vv v4, v3, v2

    # 4. ReLU clamp: max(v4, 0)
    vmax.vx v5, v4, x0

    # 5. Store output
    vse32.v v5, (t5)

    slli s1, t0, 2
    add t3, t3, s1
    add t4, t4, s1
    add t5, t5, s1
    sub s0, s0, t0
    bnez s0, transform_loop

test_done:
    ebreak

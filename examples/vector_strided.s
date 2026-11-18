# vector_strided.s - strided load gathers every other word, strided store packs them.
#
# Memory at 0x100 holds words 1, 100, 2, 200, 3, 300, 4, 400 (4 bytes each).
# Expected results after the program finishes:
#   v1 = {1, 2, 3, 4}           (vlse32.v with stride 8 bytes picks the even words)
#   Memory at 0x200..0x20F written by vsse32.v with stride 4 bytes:
#       0x200: 01 00 00 00 02 00 00 00      (double word 0x0000000200000001)
#       0x208: 03 00 00 00 04 00 00 00      (double word 0x0000000400000003)
#   x5 = 4                      (vl)

addi x10, x0, 256              # 0x100
addi x1, x0, 1
sw   x1, 0(x10)
addi x1, x0, 100
sw   x1, 4(x10)
addi x1, x0, 2
sw   x1, 8(x10)
addi x1, x0, 200
sw   x1, 12(x10)
addi x1, x0, 3
sw   x1, 16(x10)
addi x1, x0, 300
sw   x1, 20(x10)
addi x1, x0, 4
sw   x1, 24(x10)
addi x1, x0, 400
sw   x1, 28(x10)

addi x6, x0, 4
vsetvli x5, x6, e32, m1, ta, ma
addi x11, x0, 8                # load stride: 8 bytes
vlse32.v v1, (x10), x11        # v1 = {1, 2, 3, 4}

addi x12, x0, 512              # 0x200
addi x13, x0, 4                # store stride: 4 bytes (packed)
vsse32.v v1, (x12), x13

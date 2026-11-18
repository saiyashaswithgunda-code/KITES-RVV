# vector_memory.s - unit-stride vector load/store round trip.
# Open the Memory tab at 0x200 to watch the modified bytes being highlighted.
#
# Expected results after the program finishes:
#   v1 = {15, 25, 35, 45}        (e32; {10, 20, 30, 40} loaded from 0x100, plus 5)
#   Memory at 0x200..0x20F written by vse32.v:
#       0x200: 0F 00 00 00 19 00 00 00      (double word 0x000000190000000F)
#       0x208: 23 00 00 00 2D 00 00 00      (double word 0x0000002D00000023)
#   v2 = {15, 25, 35, 45}        (loaded back from 0x200)
#   x5 = 4                       (vl)

addi x10, x0, 256              # source 0x100
addi x11, x0, 512              # destination 0x200
addi x1, x0, 10
sw   x1, 0(x10)
addi x1, x0, 20
sw   x1, 4(x10)
addi x1, x0, 30
sw   x1, 8(x10)
addi x1, x0, 40
sw   x1, 12(x10)

addi x6, x0, 4
vsetvli x5, x6, e32, m1, ta, ma
vle32.v v1, (x10)              # v1 = {10, 20, 30, 40}
vadd.vi v1, v1, 5              # v1 = {15, 25, 35, 45}
vse32.v v1, (x11)              # store to 0x200
vle32.v v2, (x11)              # read it back: v2 == v1

# vector_reduction.s - single-width reductions into element 0 of the destination.
#
# Setup (e32, vl = 4): v2 = {3, 9, 4, 6}, v3 = {10, 0, 0, 0} (v3[0] is the initial value)
# Expected results after the program finishes:
#   v4[0] = 32     vredsum.vs : 10 + 3 + 9 + 4 + 6
#   v5[0] = 10     vredmax.vs : max(10, 3, 9, 4, 6)
#   v6[0] = 3      vredmin.vs : min(10, 3, 9, 4, 6)
#   x5 = 4         (vl)

addi x10, x0, 256
addi x1, x0, 3
sw   x1, 0(x10)
addi x1, x0, 9
sw   x1, 4(x10)
addi x1, x0, 4
sw   x1, 8(x10)
addi x1, x0, 6
sw   x1, 12(x10)
addi x1, x0, 10
sw   x1, 16(x10)               # v3[0] = 10, the rest of the register is zero
sw   x0, 20(x10)
sw   x0, 24(x10)
sw   x0, 28(x10)

addi x6, x0, 4
vsetvli x5, x6, e32, m1, ta, ma
vle32.v v2, (x10)              # v2 = {3, 9, 4, 6}
addi x11, x10, 16
vle32.v v3, (x11)              # v3 = {10, 0, 0, 0}

vredsum.vs v4, v2, v3          # v4[0] = 32
vredmax.vs v5, v2, v3          # v5[0] = 10
vredmin.vs v6, v2, v3          # v6[0] = 3

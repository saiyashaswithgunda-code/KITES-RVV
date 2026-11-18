# vector_mask.s - compare into mask registers, combine masks, find the first set bit.
# Only the low vl = 4 bits of each mask register are meaningful here.
#
# Setup (e32, vl = 4):  v1 = {1, 5, 3, 7}   v2 = {4, 4, 4, 4}
# Expected results after the program finishes (low 4 bits of byte 0):
#   v0 = (v1 <  v2) = 0b0101   elements 0 and 2
#   v4 = (v1 != v2) = 0b1111
#   v5 = v0 AND v4  = 0b0101
#   v6 = v0 XOR v4  = 0b1010
#   v7 = set-before-first of v6 = 0b0001   (first set bit of v6 is element 1)
#   x5 = 4          (vl)

addi x10, x0, 256
addi x1, x0, 1
sw   x1, 0(x10)
addi x1, x0, 5
sw   x1, 4(x10)
addi x1, x0, 3
sw   x1, 8(x10)
addi x1, x0, 7
sw   x1, 12(x10)

addi x6, x0, 4
vsetvli x5, x6, e32, m1, tu, mu
vle32.v v1, (x10)              # v1 = {1, 5, 3, 7}
addi x2, x0, 4
vadd.vx v2, v2, x2             # v2 = {4, 4, 4, 4}  (v2 starts at zero)

vmslt.vv v0, v1, v2            # v0 = 0b0101
vmsne.vv v4, v1, v2            # v4 = 0b1111
vmand.mm v5, v0, v4            # v5 = 0b0101
vmxor.mm v6, v0, v4            # v6 = 0b1010
vmsbf.m  v7, v6                # v7 = 0b0001

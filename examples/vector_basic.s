# vector_basic.s - configure the vector unit, load a vector and do simple arithmetic.
# Try it in the GUI: open the "Vector Registers" and "Vector CSRs" sections and step.
#
# Expected results after the program finishes:
#   x5 = 16                      (vl returned by the last vsetvli: e8, VLEN/8 = 16)
#   v1 = {1, 2, 3, 4}            (e32 elements, loaded from memory at 0x100)
#   v2 = {2, 4, 6, 8}            (v1 + v1)
#   v3 = {12, 14, 16, 18}        (v2 + 10)  -> view it as 16 bytes after the final vsetvli:
#        0C 00 00 00 0E 00 00 00 10 00 00 00 12 00 00 00
#   Vector CSRs at the end: vl = 16, vtype = 0xC0 (e8, m1, ta, ma), VLMAX = 16

addi x10, x0, 256              # base address 0x100
addi x1, x0, 1
sw   x1, 0(x10)
addi x1, x0, 2
sw   x1, 4(x10)
addi x1, x0, 3
sw   x1, 8(x10)
addi x1, x0, 4
sw   x1, 12(x10)

addi x6, x0, 4                 # AVL = 4 elements
vsetvli x5, x6, e32, m1, ta, ma
vle32.v v1, (x10)              # v1 = {1, 2, 3, 4}
vadd.vv v2, v1, v1             # v2 = {2, 4, 6, 8}
vadd.vi v3, v2, 10             # v3 = {12, 14, 16, 18}

addi x6, x0, 16                # re-interpret the registers as 16 bytes
vsetvli x5, x6, e8, m1, ta, ma

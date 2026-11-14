Work Distribution

The implementation work is divided between two team members based on instruction families and implementation complexity.

Person 1: Core Integer Operations and Basic Memory

1. Basic Integer Arithmetic
   - Addition and subtraction: vadd, vsub, vrsub
   - Minimum and maximum: vmin[u], vmax[u]
   - Bitwise operations: vand, vor, vxor
   - Shifts: vsll, vsrl, vsra
   - Multiplication: vmul, vmulh, vmulhu, vmulhsu
   - Division and remainder: vdiv[u], vrem[u]
2. Comparison Instructions
   - vmseq, vmsne
   - vmslt[u], vmsle[u], vmsgt[u]
3. Basic Memory Instructions
   - Unit-stride: vle, vse
   - Strided: vlse, vsse
   - Fault-only-first loads: vle...ff
4. Mask Operations and Semantics
   - Mask logical operations: vmand, vmandn, vmor, vmorn, vmxor
   - Mask prefix operations: vmsbf, vmsif, vmsof
   - Mask memory operations: vlm.v, vsm.v
   - Masked execution and tail-policy handling
5. GUI Debugging
   - Display vector registers v0–v31.
   - Display vector CSRs: vl, vtype, vlenb, vstart.

Person 2: Advanced Integer Operations and Vector Data Movement

1. Advanced Integer Arithmetic
   - Widening arithmetic: vwaddu, vwadd, vwsubu, vwsub
   - Widening multiplication: vwmul, vwmulu, vwmulsu
   - Multiply-accumulate: vmacc, vnmsac, vmadd, vnmsub
   - Widening multiply-accumulate: vwmacc, vwmaccu
   - Saturating arithmetic: vsadd[u], vssub[u]
   - Averaging operations: vaadd[u], vasub[u]
   - Fixed-point operations: vsmul, vnclip[u]
   - Carry and borrow: vadc, vmadc, vsbc, vmsbc
2. Advanced Memory Instructions
   - Indexed: vluxei, vloxei, vsuxei, vsoxei
   - Unit-stride segments: vlseg, vsseg
   - Strided segments: vlsseg, vssseg
   - Indexed segments: vluxseg, vloxseg, vsuxseg, vsoxseg
   - Whole-register operations: vl1re–vl8re, vs1r–vs8r
3. Vector Reductions
   - vredsum
   - vredand, vredor, vredxor
   - vredmin[u], vredmax[u]
   - Widening reductions: vwredsum[u]
4. Vector Permutation and Movement
   - Slide operations: vslideup, vslidedown, vslide1up, vslide1down
   - Gather operations: vrgather, vrgatherei16
   - Compression: vcompress
   - Merge and move operations: vmerge, vmv variants
5. Advanced Mask Operations
   - vmnand, vmnor, vmxnor
   - vcpop, vfirst
   - viota, vid

Shared Responsibilities

Both team members will contribute to the following activities:

- Integration of instruction implementations into KITES.
- Support for vector configuration instructions: vsetvli, vsetivli, and vsetvl.
- Correct handling of SEW, LMUL, vector length, masking, tail policies, and vector architectural state.
- Development of unit tests, execution tests, and regression tests.
- Integration testing and debugging across instruction families.
- Documentation of supported features, limitations, and test results.

Future Extensions

After the integer instruction subset is implemented and verified, the following features may be considered:

- Vector floating-point arithmetic, comparisons, conversions, and reductions.
- Additional processor-model support.
- Randomized testing and reference validation against Sail or Spike.
- Further GUI debugging and visualization improvements.
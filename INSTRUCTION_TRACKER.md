# RVV Support in KITES --- Instruction Tracker

## 1. Legend

-   **Complete**: assembler, encoding, execution, and relevant tests are
    implemented for the documented subset.
-   **Pending**: not yet implemented.
-   **Optional**: possible extension, not required by the current
    subset.
-   **Restricted**: instruction family exists only under the constraints
    stated.

## 2. Implemented instructions

  -----------------------------------------------------------------------------------------
  Instruction   Class /           Assembler    Encoding        RVSS       Tests Status
                operation                                 execution
  ------------- --------------- ----------- ----------- ----------- ----------- -----------
  `vsetvli`     Vector                  Yes         Yes         Yes         Yes Complete
                configuration                                                   for
                                                                                supported
                                                                                VTYPE

  `vadd.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                add

  `vsub.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                subtract

  `vand.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                AND

  `vor.vv`      Vector-vector           Yes         Yes         Yes         Yes Complete
                OR

  `vxor.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                XOR

  `vmin.vv`     Vector-vector signed    Yes         Yes         Yes         Yes Complete
                minimum

  `vminu.vv`    Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                minimum

  `vmax.vv`     Vector-vector signed    Yes         Yes         Yes         Yes Complete
                maximum

  `vmaxu.vv`    Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                maximum

  `vadd.vx`     Vector-scalar add       Yes         Yes         Yes         Yes Complete

  `vsub.vx`     Vector-scalar subtract  Yes         Yes         Yes         Yes Complete

  `vrsub.vx`    Vector-scalar reverse   Yes         Yes         Yes         Yes Complete
                subtract

  `vmin.vx`     Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                minimum

  `vminu.vx`    Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                minimum

  `vmax.vx`     Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                maximum

  `vmaxu.vx`    Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                maximum

  `vadd.vi`     Vector-immediate add    Yes         Yes         Yes         Yes Complete

  `vrsub.vi`    Vector-immediate        Yes         Yes         Yes         Yes Complete
                reverse subtract

  `vredsum.vs`  Vector sum reduction    Yes         Yes         Yes         Yes Complete

  `vle8.v`      Unit-stride             Yes         Yes         Yes         Yes Complete
                load, EEW=8

  `vle16.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                load, EEW=16

  `vle32.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                load, EEW=32

  `vle64.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                load, EEW=64

  `vse8.v`      Unit-stride             Yes         Yes         Yes         Yes Complete
                store, EEW=8

  `vse16.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                store, EEW=16

  `vse32.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                store, EEW=32

  `vse64.v`     Unit-stride             Yes         Yes         Yes         Yes Complete
                store, EEW=64

  `vsll.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                logical shift left

  `vsll.vx`     Vector-scalar           Yes         Yes         Yes         Yes Complete
                logical shift left

  `vsll.vi`     Vector-immediate        Yes         Yes         Yes         Yes Complete
                logical shift left

  `vsrl.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                logical shift right

  `vsrl.vx`     Vector-scalar           Yes         Yes         Yes         Yes Complete
                logical shift right

  `vsrl.vi`     Vector-immediate        Yes         Yes         Yes         Yes Complete
                logical shift right

  `vsra.vv`     Vector-vector           Yes         Yes         Yes         Yes Complete
                arithmetic shift right

  `vsra.vx`     Vector-scalar           Yes         Yes         Yes         Yes Complete
                arithmetic shift right

  `vsra.vi`     Vector-immediate        Yes         Yes         Yes         Yes Complete
                arithmetic shift right

  `vmul.vv`     Vector-vector integer   Yes         Yes         Yes         Yes Complete
                multiply (low half)

  `vmul.vx`     Vector-scalar integer   Yes         Yes         Yes         Yes Complete
                multiply (low half)

  `vmulh.vv`    Vector-vector signed    Yes         Yes         Yes         Yes Complete
                multiply (high half)

  `vmulh.vx`    Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                multiply (high half)

  `vmulhu.vv`   Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                multiply (high half)

  `vmulhu.vx`   Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                multiply (high half)

  `vmulhsu.vv`  Vector-vector signed *  Yes         Yes         Yes         Yes Complete
                unsigned multiply
                (high half)

  `vmulhsu.vx`  Vector-scalar signed *  Yes         Yes         Yes         Yes Complete
                unsigned multiply
                (high half)

  `vdivu.vv`    Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                division

  `vdivu.vx`    Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                division

  `vdiv.vv`     Vector-vector signed    Yes         Yes         Yes         Yes Complete
                division

  `vdiv.vx`     Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                division

  `vremu.vv`    Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                remainder

  `vremu.vx`    Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                remainder

  `vrem.vv`     Vector-vector signed    Yes         Yes         Yes         Yes Complete
                remainder

  `vrem.vx`     Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                remainder

  `vmseq.vv`    Vector-vector equal     Yes         Yes         Yes         Yes Complete
                comparison

  `vmseq.vx`    Vector-scalar equal     Yes         Yes         Yes         Yes Complete
                comparison

  `vmseq.vi`    Vector-immediate equal  Yes         Yes         Yes         Yes Complete
                comparison

  `vmsne.vv`    Vector-vector not-equal Yes         Yes         Yes         Yes Complete
                comparison

  `vmsne.vx`    Vector-scalar not-equal Yes         Yes         Yes         Yes Complete
                comparison

  `vmsne.vi`    Vector-immediate        Yes         Yes         Yes         Yes Complete
                not-equal comparison

  `vmsltu.vv`   Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                less-than comparison

  `vmsltu.vx`   Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                less-than comparison

  `vmslt.vv`    Vector-vector signed    Yes         Yes         Yes         Yes Complete
                less-than comparison

  `vmslt.vx`    Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                less-than comparison

  `vmsleu.vv`   Vector-vector unsigned  Yes         Yes         Yes         Yes Complete
                less-than-or-equal

  `vmsleu.vx`   Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                less-than-or-equal

  `vmsleu.vi`   Vector-immediate        Yes         Yes         Yes         Yes Complete
                unsigned
                less-than-or-equal

  `vmsle.vv`    Vector-vector signed    Yes         Yes         Yes         Yes Complete
                less-than-or-equal

  `vmsle.vx`    Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                less-than-or-equal

  `vmsle.vi`    Vector-immediate signed Yes         Yes         Yes         Yes Complete
                less-than-or-equal

  `vmsgtu.vx`   Vector-scalar unsigned  Yes         Yes         Yes         Yes Complete
                greater-than comparison

  `vmsgtu.vi`   Vector-immediate        Yes         Yes         Yes         Yes Complete
                unsigned greater-than
                comparison

  `vmsgt.vx`    Vector-scalar signed    Yes         Yes         Yes         Yes Complete
                greater-than comparison

  `vmsgt.vi`    Vector-immediate signed Yes         Yes         Yes         Yes Complete
                greater-than comparison

  `vmand.mm`    Vector-vector mask AND  Yes         Yes         Yes         Yes Complete

  `vmandn.mm`   Vector-vector mask AND  Yes         Yes         Yes         Yes Complete
                with inverted source 2

  `vmor.mm`     Vector-vector mask OR   Yes         Yes         Yes         Yes Complete

  `vmorn.mm`    Vector-vector mask OR   Yes         Yes         Yes         Yes Complete
                with inverted source 2

  `vmxor.mm`    Vector-vector mask XOR  Yes         Yes         Yes         Yes Complete

  `vmsbf.m`     Vector mask set-before- Yes         Yes         Yes         Yes Complete
                first mask bit

  `vmsof.m`     Vector mask set-only-   Yes         Yes         Yes         Yes Complete
                first mask bit

  `vmsif.m`     Vector mask set-        Yes         Yes         Yes         Yes Complete
                including-first mask
                bit
  -----------------------------------------------------------------------------------------

## 3. Pending arithmetic candidates

These are suggested additions, not a commitment that every instruction
must be implemented. Check the RVV 1.0 specification before assigning or
implementing them.

All 20 legal forms of vector comparison instructions (`vmseq`, `vmsne`, `vmslt`,
`vmsltu`, `vmsle`, `vmsleu`, `vmsgt`, `vmsgtu`) have been implemented in Batch 1.

Additional candidate families for future implementation:
- Saturating integer arithmetic (`vsadd`, `vssub`, etc.)
- Integer merge/move (`vmerge`, `vmv`)

Note: do not assume `vsub.vi` is a valid substitute for `vrsub.vi`.
Verify mnemonic availability and operand semantics in the specification.

## 4. Mask operations

Implemented in Batch 2:

  Instruction   Operation                                      Status
  ------------- ---------------------------------------------- ---------
  `vmand.mm`    Mask AND                                       Complete
  `vmandn.mm`   Mask AND with inverted source 2                Complete
  `vmor.mm`     Mask OR                                        Complete
  `vmorn.mm`    Mask OR with inverted source 2                 Complete
  `vmxor.mm`    Mask XOR                                       Complete
  `vmsbf.m`     Mask set-before-first                          Complete
  `vmsof.m`     Mask set-only-first                            Complete
  `vmsif.m`     Mask set-including-first                       Complete

Pending / deferred mask logical operations:

  Instruction   Operation                                      Status
  ------------- ---------------------------------------------- ---------
  `vmnand.mm`   Mask NAND                                      Pending
  `vmnor.mm`    Mask NOR                                       Pending
  `vmxnor.mm`   Mask XNOR                                      Pending
  `vmnot.m`     Mask NOT (pseudo for `vmorn.mm vd, vs, vs`)    Pending

These instructions operate on packed mask bits. Active elements are computed
bitwise on mask registers. Mask prefix instructions scan for the first active
set bit, ignoring inactive elements (where `v0[i] == 0`). Tail elements in
`[vl, VLMAX)` are set to 1 (deterministic tail agnostic policy).

## 5. Optional memory extensions

  -----------------------------------------------------------------------
  Family                  Example mnemonics       Status
  ----------------------- ----------------------- -----------------------
  Strided loads           `vlse8.v`, `vlse16.v`,  Optional / pending
                          `vlse32.v`, `vlse64.v`

  Strided stores          `vsse8.v`, `vsse16.v`,  Optional / pending
                          `vsse32.v`, `vsse64.v`

  Indexed unordered loads `vluxei*.v`             Optional / pending

  Indexed ordered loads   `vloxei*.v`             Optional / pending

  Indexed unordered       `vsuxei*.v`             Optional / pending
  stores

  Indexed ordered stores  `vsoxei*.v`             Optional / pending
  -----------------------------------------------------------------------

These require additional operand syntax, instruction fields, and
address-generation semantics. Treat them as a separate milestone.

## 6. Contributor assignment

Fill in the owner and status when work is assigned. Keep one row per
instruction or coherent instruction family.

  --------------------------------------------------------------------------------
  Instruction /     Assigned to  Branch / PR Status      Tests added Notes
  feature
  ----------------- ------------ ----------- ----------- ----------- -------------
  `vadd.vx`,        Completed    `rvv-       Complete    Vector-     SEW 8/16/32/64,
  `vsub.vx`                      development2`           Arithmetic  scalar mod
                                                         Test        2^SEW, aliasing

  `vadd.vi`,        Completed    `rvv-       Complete    Vector-     simm5 [-16,15],
  `vrsub.vi`                     development2`           Arithmetic  range checks,
                                                         Test,       OPIVI format
                                                         Vector-
                                                         Assembler
                                                         Test

  `vrsub.vx`,       Completed    `rvv-       Complete    Vector-     std::bit_cast
  `vmin[u]`,                     development2`           Arithmetic  signed compare,
  `vmax[u]`                                              Test        SEW 8/16/32/64

  `vredsum.vs`      Completed    `rvv-       Complete    Vector-     Single-width
                                 development2`           Reduction   sum reduction
                                                         Test        into vd[0]

  Vector            Completed    `rvv-       Complete    Vector-     All 20 legal
  comparisons                    development2`           Arithmetic  forms (.vv/.vx/.vi),
  (`vmseq`,                                              Test,       realigned to standard
  `vmsne`, `vmslt`,                                      Vector-     OPIVV/OPIVX/OPIVI,
  `vmsltu`, `vmsle`,                                     Assembler   packed mask vd,
  `vmsleu`, `vmsgt`,                                     Test        tail agnostic to
  `vmsgtu`)                                                          VLMAX, vstart>=vl

  Mask logical &    Completed    `rvv-       Complete    Vector-     Packed mask bits,
  prefix                         development2`           Arithmetic  OPMVV funct3=010,
  operations                                             Test,       tail agnostic to
  (`vmand.mm`,                                           Vector-     VLMAX, vstart>=vl,
  `vmandn.mm`,                                           Assembler   prefix active scan,
  `vmor.mm`,                                             Test        overlap & edge cases
  `vmorn.mm`,
  `vmxor.mm`,
  `vmsbf.m`,
  `vmsof.m`,
  `vmsif.m`)

  Shift             Completed    `rvv-       Complete    Vector-     Shift amount
  operations                     development2`           Arithmetic  mod SEW, uimm5
  (`vsll`, `vsrl`,                                       Test,       for .vi, SEW
  `vsra`)                                                Vector-     8/16/32/64
                                                         Assembler
                                                         Test

  Vector integer    Completed    `rvv-       Complete    Vector-     SEW 8/16/32/64,
  multiplication                 development2`           Arithmetic  OPMVV/OPMVX,
  (`vmul`, `vmulh`,                                      Test,       exact 128-bit
  `vmulhu`,                                              Vector-     signed/unsigned
  `vmulhsu`)                                             Assembler   high half
                                                         Test

  Vector integer    Completed    `rvv-       Complete    Vector-     SEW 8/16/32/64,
  division &                     development2`           Arithmetic  div/rem by zero,
  remainder                                              Test,       INT_MIN / -1 safe,
  (`vdiv`, `vdivu`,                                      Vector-     OPMVV/OPMVX,
  `vrem`, `vremu`)                                       Assembler   vstart >= vl fix
                                                         Test

  Strided/indexed   Unassigned   ---         Optional    ---         Separate
  memory                                                             larger phase
  --------------------------------------------------------------------------------

## 7. Rules for updating this tracker

When an instruction is implemented: 1. Update assembler, encoding,
execution, and tests columns separately. 2. Do not mark it complete if
only a hand-encoded test executes. 3. Record unsupported forms and
constraints in Notes. 4. Record exact test names or test file. 5. Update
`IMPLEMENTATION_STATUS.md` and `ARCHITECTURE.md` if the design changes.

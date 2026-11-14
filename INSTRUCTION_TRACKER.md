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
  -----------------------------------------------------------------------------------------

## 3. Pending arithmetic candidates

These are suggested additions, not a commitment that every instruction
must be implemented. Check the RVV 1.0 specification before assigning or
implementing them.

  Instruction   Operation                           Suggested priority Status
  ------------- --------------------------------- -------------------- ---------
  `vadd.vx`     Vector-scalar add                                 High Pending
  `vsub.vx`     Vector-scalar subtract                            High Pending
  `vadd.vi`     Vector-immediate add                              High Pending
  `vrsub.vi`    Reverse subtract with immediate                 Medium Pending
  `vmseq.vv`    Vector equality comparison                      Medium Pending
  `vmsne.vv`    Vector inequality comparison                    Medium Pending
  `vmslt.vv`    Signed less-than comparison                     Medium Pending

Note: do not assume `vsub.vi` is a valid substitute for `vrsub.vi`.
Verify mnemonic availability and operand semantics in the specification.

## 4. Pending mask operations

Potential mask logical operations:

  Instruction   Operation   Status
  ------------- ----------- ---------
  `vmand.mm`    Mask AND    Pending
  `vmor.mm`     Mask OR     Pending
  `vmxor.mm`    Mask XOR    Pending
  `vmnot.m`     Mask NOT    Pending

These instructions operate on packed mask bits. They need a mask-aware
execution path and tests; ordinary SEW-wide arithmetic handling is not
automatically suitable.

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
  `vadd.vx`,        Unassigned   ---         Pending     ---         First
  `vsub.vx`                                                          recommended
                                                                     extension

  `vadd.vi`,        Unassigned   ---         Pending     ---         Confirm
  `vrsub.vi`                                                         immediate
                                                                     semantics

  Vector            Unassigned   ---         Pending     ---         Mask
  comparisons                                                        destination

  Mask logical      Unassigned   ---         Pending     ---         Packed mask
  operations                                                         bit
                                                                     operations

  Strided/indexed   Unassigned   ---         Optional    ---         Separate
  memory                                                             larger phase
  --------------------------------------------------------------------------------

## 7. Rules for updating this tracker

When an instruction is implemented: 1. Update assembler, encoding,
execution, and tests columns separately. 2. Do not mark it complete if
only a hand-encoded test executes. 3. Record unsupported forms and
constraints in Notes. 4. Record exact test names or test file. 5. Update
`IMPLEMENTATION_STATUS.md` and `ARCHITECTURE.md` if the design changes.

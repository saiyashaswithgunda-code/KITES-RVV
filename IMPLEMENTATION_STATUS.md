# RVV Support in KITES --- Implementation Status

## 1. Status summary

**Latest known verification:** 387/387 tests passed (100% CTest pass rate).

The statuses below describe the implementation after Batch 1 vector comparisons and Batch 5 integer division and remainder. A
new contributor should run the full suite before making changes to
confirm the current checkout still matches this baseline.

  -----------------------------------------------------------------------
  Area                    Status                  Notes
  ----------------------- ----------------------- -----------------------
  Vector register file    Complete                32 × 128-bit vector
                                                  registers

  Typed vector element    Complete                8/16/32/64-bit element
  access                                          access

  Vector CSRs             Complete for current    `vstart`, `vl`,
                          subset                  `vtype`, `vlenb`

  VTYPE decoding          Complete for current    LMUL=m1; SEW=8/16/32/64
                          subset                  

  `vsetvli` execution     Complete for current    AVL rules and invalid
                          subset                  VTYPE handling

  `vsetvli`               Complete                Parser and machine-code
  assembler/encoding                              tests

  Vector-vector integer   Complete                Basic arithmetic (vadd,
  arithmetic                                      vsub, vand, vor, vxor)
                                                  and min/max (vmin, vminu,
                                                  vmax, vmaxu)

  Vector-scalar integer   Complete                vadd.vx, vsub.vx,
  arithmetic                                      vrsub.vx, vmin[u].vx,
                                                  vmax[u].vx

  Vector-immediate        Complete                vadd.vi, vrsub.vi with
  integer arithmetic                              simm5 immediate

  Vector shift            Complete                vsll.[vv,vx,vi],
  operations                                      vsrl.[vv,vx,vi],
                                                  vsra.[vv,vx,vi]

  Vector integer          Complete                vmul.[vv,vx],
  multiplication                                  vmulh.[vv,vx],
                                                  vmulhu.[vv,vx],
                                                  vmulhsu.[vv,vx]

  Vector integer division Complete                vdiv.[vv,vx],
  & remainder                                     vdivu.[vv,vx],
                                                  vrem.[vv,vx],
                                                  vremu.[vv,vx]

  Vector single-width     Complete                vredsum.vs across active
  reduction                                       elements to vd[0]

  Unit-stride vector      Complete for current    Eight load/store
  memory                  subset                  variants

  Masked execution        Complete for            `vm=0` and `vm=1`
                          implemented operations  

  Tail/mask policy        Complete for            Deterministic agnostic
  behavior                implemented operations  behavior

  Undo/redo               Complete for covered    Verify for each new
                          vector state changes    instruction

  Additional arithmetic   Pending                 Saturating arithmetic,
  families                                        fixed-point arithmetic

  Vector comparisons      Complete                All 20 legal forms (.vv, .vx, .vi),
                                                  packed mask vd, tail agnostic to
                                                  VLMAX, SEW 8/16/32/64

  Vector mask logical     Pending                 See Section 4
  instructions                                    

  Strided/indexed memory  Pending / optional      Not part of current
                                                  memory subset

  Fractional and grouped  Unsupported             Current decoder accepts
  LMUL                                            only m1

  Fault/trap-accurate     Not established         Inspect existing KITES
  vector memory                                   exception model before
                                                  implementing

  Full RVV 1.0 compliance Not claimed             This is a selected
                                                  subset
  -----------------------------------------------------------------------

## 2. Implemented instructions

### Configuration

-   `vsetvli`

The implementation supports the current project's `m1` configuration and
SEW values 8, 16, 32, and 64 bits.

### Vector-vector arithmetic

-   `vadd.vv`
-   `vsub.vv`
-   `vand.vv`
-   `vor.vv`
-   `vxor.vv`
-   `vmin.vv`
-   `vminu.vv`
-   `vmax.vv`
-   `vmaxu.vv`

Current implementation supports:
- SEW = 8, 16, 32, 64. LMUL = m1.
- Configured `vl` and `vstart`.
- Signed and unsigned comparisons for min/max via bit-preserving `std::bit_cast`.
- Masked and unmasked execution (`vm=0` and `vm=1`).
- Tail (`vta`) and mask (`vma`) policy behavior.
- Source/destination overlap handled by snapshotting source values before destination updates.

### Vector-scalar arithmetic

-   `vadd.vx`
-   `vsub.vx`
-   `vrsub.vx`
-   `vmin.vx`
-   `vminu.vx`
-   `vmax.vx`
-   `vmaxu.vx`

Current implementation supports:
- Scalar operand extracted from `rs1` as `scalar = rs1 mod 2^SEW`.
- Signed and unsigned comparisons for min/max via bit-preserving `std::bit_cast` across all SEWs (8, 16, 32, 64).
- `vrsub.vx` calculates `scalar - vs2[i]`.
- SEW = 8, 16, 32, 64. LMUL = m1.
- Source snapshotting of `vs2`, `rs1`, and mask `v0` before modifying `vd`.
- Masked execution, tail and mask policy behavior, and `vstart` semantics.

### Vector-immediate arithmetic

-   `vadd.vi`
-   `vrsub.vi`

Current implementation supports:
- 5-bit signed immediate `simm5` parsed in range `[-16, 15]`, sign-extended to SEW bits.
- `vadd.vi` calculates `vs2[i] + simm5`.
- `vrsub.vi` calculates `simm5 - vs2[i]`.
- Assembler validation and rejection of out-of-range immediates.
- Masked execution (`vm=0`), tail/mask policies, and undo/redo.

### Vector shift operations

-   `vsll.vv`, `vsll.vx`, `vsll.vi`
-   `vsrl.vv`, `vsrl.vx`, `vsrl.vi`
-   `vsra.vv`, `vsra.vx`, `vsra.vi`

Current implementation supports:
- Shift amounts masked to low $\lg(\text{SEW})$ bits across `.vv`, `.vx`, and `.vi` forms.
- `.vi` variants parse 5-bit unsigned immediate `uimm5` in range `[0, 31]`.
- Proper arithmetic right shift (`vsra`) with sign bit preservation for each SEW.
- SEW = 8, 16, 32, 64. LMUL = m1.
- Source snapshotting, masking (`vm=0`/`vm=1`), tail/mask policies, and undo/redo.

### Vector integer multiplication

-   `vmul.vv`, `vmul.vx`
-   `vmulh.vv`, `vmulh.vx`
-   `vmulhu.vv`, `vmulhu.vx`
-   `vmulhsu.vv`, `vmulhsu.vx`

Current implementation supports:
- `vmul` returns low SEW bits using unsigned modular arithmetic.
- `vmulh` returns upper SEW bits of signed $\times$ signed multiplication.
- `vmulhu` returns upper SEW bits of unsigned $\times$ unsigned multiplication.
- `vmulhsu` returns upper SEW bits of signed(`vs2`) $\times$ unsigned(`vs1/rs1`) multiplication.
- SEW = 8, 16, 32, 64; LMUL = m1.
- Scalar operands in `.vx` variants are masked/truncated to active SEW and interpreted with instruction-specific signedness.
- Exact wide arithmetic (using `unsigned __int128` and signed `__int128` for SEW=64) avoiding signed overflow and undefined behavior.
- Source/destination overlap and hazard safety via operand and mask snapshotting.
- Masked execution (`vm=0`/`vm=1`), tail policies (`ta`/`tu`), mask policies (`ma`/`mu`), `vstart` boundary semantics, and delta undo/redo.

### Vector integer division and remainder

-   `vdiv.vv`, `vdiv.vx`
-   `vdivu.vv`, `vdivu.vx`
-   `vrem.vv`, `vrem.vx`
-   `vremu.vv`, `vremu.vx`

Current implementation supports:
- `vdiv` computes signed division rounding toward zero; division by zero returns all ones (`-1`), and signed overflow (`INT_MIN / -1`) returns `INT_MIN` without triggering C++ undefined behavior or divide-error exceptions.
- `vdivu` computes unsigned division; division by zero returns all ones.
- `vrem` computes signed remainder with sign of dividend; remainder by zero returns dividend, and signed overflow (`INT_MIN % -1`) returns `0`.
- `vremu` computes unsigned remainder; remainder by zero returns dividend.
- SEW = 8, 16, 32, 64; LMUL = m1.
- Scalar operands in `.vx` variants are masked/truncated to active SEW and interpreted with instruction-specific signedness.
- Masked execution (`vm=0`/`vm=1`), tail policies (`ta`/`tu`), mask policies (`ma`/`mu`), and delta undo/redo.
- `vstart` boundary semantics including $vstart \ge vl$ handling where no elements (body or tail) are updated and `vstart` is reset to 0.

### Vector comparison instructions

-   `vmseq.vv`, `vmseq.vx`, `vmseq.vi` (equal)
-   `vmsne.vv`, `vmsne.vx`, `vmsne.vi` (not equal)
-   `vmsltu.vv`, `vmsltu.vx` (unsigned less than)
-   `vmslt.vv`, `vmslt.vx` (signed less than)
-   `vmsleu.vv`, `vmsleu.vx`, `vmsleu.vi` (unsigned less than or equal)
-   `vmsle.vv`, `vmsle.vx`, `vmsle.vi` (signed less than or equal)
-   `vmsgtu.vx`, `vmsgtu.vi` (unsigned greater than)
-   `vmsgt.vx`, `vmsgt.vi` (signed greater than)

Current implementation supports:
- All 20 legal instruction/operand-form combinations (no unsupported forms like `.vi` for `vmslt`/`vmsltu` or `.vv` for `vmsgt`/`vmsgtu`).
- Comparison result writes boolean predicate bits packed bit-by-bit into mask destination register `vd` (bit $i$ corresponds to element $i$).
- Supported SEW = 8, 16, 32, 64; LMUL = m1; VLEN = 128.
- Encodings: OPMVV (`funct3 = 010`), OPMVX (`funct3 = 110`), OPIVI (`funct3 = 011`).
- Scalar operands in `.vx` variants are masked/truncated to active SEW and interpreted with instruction-specific signedness.
- Immediate operands in `.vi` variants parse 5-bit signed immediate `simm5` in `[-16, 15]`, sign-extended to SEW bits.
- Deterministic mask tail policy: mask destination is always tail-agnostic (tail bits in $[vl, VLMAX)$ are set to 1 regardless of `vta`); bits beyond $VLMAX$ remain strictly undisturbed.
- Masking policy: inactive elements under `vm=0` remain undisturbed when `vma=0`, and are overwritten with 1 when `vma=1`.
- Hazard-free register overlap: source registers (`vs1`, `vs2`, `rs1`) and execution mask (`v0`) are snapshotted prior to destination updates, ensuring safe execution even when $vd = v0$, $vd = vs1$, or $vd = vs2$.
- Boundary semantics: $vstart \ge vl$ produces no body or tail modifications, leaves the destination register completely untouched, and resets `vstart` to 0 on writeback.
- Full delta tracking and undo/redo support.

### Vector reduction

-   `vredsum.vs`

Current implementation supports:
- Sum reduction across active elements in `vs2` added to scalar element `vs1[0]`.
- Result written to `vd[0]`.
- Inactive elements beyond `vl` follow tail policy `vta`.
- Supported for SEW = 8, 16, 32, 64. LMUL = m1.

### Unit-stride vector memory

Loads: - `vle8.v` - `vle16.v` - `vle32.v` - `vle64.v`

Stores: - `vse8.v` - `vse16.v` - `vse32.v` - `vse64.v`

Current implementation is limited to: - Unit stride. - Single field
(`nf=0`). - `mew=0`. - `mop=0`. - `lumop/sumop=0`. - `vm=0` or `vm=1`. -
SEW must match the memory element width. - LMUL=m1.

## 3. Known implementation constraints

These constraints are intentional unless a future contributor explicitly
expands the scope:

1.  VLEN is fixed at 128 bits.
2.  LMUL is restricted to m1. The VTYPE decoder marks unsupported LMUL
    encodings invalid.
3.  SEW is restricted to 8, 16, 32, and 64 bits.
4.  Vector arithmetic currently supports the vector-vector, vector-scalar,
    vector-immediate integer operations, and single-width sum reduction
    listed above.
5.  Current memory support is unit-stride and single-field only.
6.  The current memory execution path uses the existing RVSS memory
    controller and current-delta tracking. Do not assume it already
    implements all architectural exception and fault semantics required
    by the complete RVV specification.
7.  Full RVV 1.0 compliance is not claimed.

## 4. Pending instruction families and suggested order

The following are candidates for extension. Verify exact instruction
encodings, operand forms, and semantics against the RVV 1.0
specification before coding.

### Priority A --- Vector comparisons (Implemented)

Implemented additions (all 20 legal forms):
- `vmseq.vv`, `vmseq.vx`, `vmseq.vi` (equal)
- `vmsne.vv`, `vmsne.vx`, `vmsne.vi` (not equal)
- `vmslt.vv`, `vmslt.vx` (signed less than)
- `vmsltu.vv`, `vmsltu.vx` (unsigned less than)
- `vmsle.vv`, `vmsle.vx`, `vmsle.vi` (signed less than or equal)
- `vmsleu.vv`, `vmsleu.vx`, `vmsleu.vi` (unsigned less than or equal)
- `vmsgt.vx`, `vmsgt.vi` (signed greater than)
- `vmsgtu.vx`, `vmsgtu.vi` (unsigned greater than)

Comparison instructions write boolean predicate bits to a vector mask destination
register (`vd`) packed bit-by-bit. Tail bits in $[vl, VLMAX)$ are set to 1 (deterministic
tail-agnostic implementation choice), while bits beyond $VLMAX$ remain undisturbed. Fully
verified across SEW 8, 16, 32, 64, register overlaps ($vd=v0, vd=vs1, vd=vs2$), $vstart \ge vl$,
and undo/redo.

### Priority B --- Vector mask logical operations

Potential family:
- `vmand.mm`
- `vmor.mm`
- `vmxor.mm`
- `vmnot.m`
- `vmnand.mm`
- `vmnor.mm`
- `vmxnor.mm`

These operate on packed mask bits and require dedicated handling; do not route
them through ordinary SEW arithmetic without checking the specification.

### Priority C --- Vector shift operations (Implemented)

Implemented additions:
- `vsll.vv`, `vsll.vx`, `vsll.vi` (logical shift left)
- `vsrl.vv`, `vsrl.vx`, `vsrl.vi` (logical shift right)
- `vsra.vv`, `vsra.vx`, `vsra.vi` (arithmetic shift right)

Shift amount is taken from the low $\lg(\text{SEW})$ bits of the second operand. Immediate variants use zero-extended 5-bit unsigned immediate (`uimm5`).
Supported shift amount is masked to log2(SEW) bits for all three operand forms

### Priority D --- Vector integer multiply and divide (Implemented)

Implemented multiply additions:
- `vmul.vv`, `vmul.vx`
- `vmulh.vv`, `vmulh.vx`, `vmulhu.vv`, `vmulhu.vx`, `vmulhsu.vv`, `vmulhsu.vx`

Implemented integer divide/remainder additions:
- `vdiv.vv`, `vdiv.vx`, `vdivu.vv`, `vdivu.vx`
- `vrem.vv`, `vrem.vx`, `vremu.vv`, `vremu.vx`

### Priority E --- Additional memory addressing (optional)

Potential additions: - Strided loads/stores (`vlse*.v`, `vsse*.v`). -
Indexed loads/stores (`vluxei*.v`, `vloxei*.v`, `vsuxei*.v`,
`vsoxei*.v`).

These require decoding additional memory fields and calculating
per-element addresses. They are a larger scope increase than adding
arithmetic instructions.

## 5. Important semantic checklist for every new instruction

Before marking a new vector instruction complete, test:

-   Correct instruction mnemonic and operand syntax.
-   Correct 32-bit machine encoding.
-   Correct decode and instruction dispatch.
-   Valid/invalid VTYPE behavior.
-   All supported SEW values where applicable.
-   `vl=0`, `vl=1`, and `vl=VLMAX`.
-   `vstart=0`, nonzero `vstart`, and `vstart=vl`.
-   Masked and unmasked forms where the instruction supports masking.
-   `vma=0` and `vma=1` where destination mask policy is relevant.
-   `vta=0` and `vta=1` where tail policy is relevant.
-   Source/destination overlap and any architectural overlap
    restrictions.
-   No unintended changes to inactive, prestart, or tail elements.
-   `vstart` reset behavior after successful completion.
-   Undo and redo of every modified architectural state component.
-   Regression: full test suite.

## 6. Suggested completion definition

An instruction is not complete merely because a hand-encoded instruction
executes. A complete addition should include:

1.  Instruction metadata.
2.  Parser support.
3.  Machine-code generation.
4.  Exact encoding tests.
5.  RVSS execution support.
6.  Positive and negative execution tests.
7.  Mask/tail/VSTART behavior as applicable.
8.  Undo/redo tests where applicable.
9.  Full regression suite passing.
10. Documentation and tracker update.

# RVV Support in KITES --- Implementation Status

## 1. Status summary

**Latest known verification:** 435/435 tests passed (100% CTest pass rate).

The statuses below describe the implementation after Batch 1 vector comparisons (realigned to RVV 1.0 OPIVV/OPIVX/OPIVI encodings), Batch 2 mask logical and prefix operations, Batch 3 strided and mask memory instructions, and Batch 5 integer division and remainder. A
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
                                                  standard RVV 1.0 encodings,
                                                  packed mask vd, tail agnostic to
                                                  VLMAX, SEW 8/16/32/64

  Vector mask logical     Complete                5 instructions (vmand, vmandn,
  instructions                                    vmor, vmorn, vmxor.mm)

  Vector mask prefix      Complete                3 instructions (vmsbf, vmsof,
  instructions                                    vmsif.m), unmasked & masked

  Strided vector memory   Complete                8 instructions (vlse8/16/32/64.v,
                                                  vsse8/16/32/64.v), signed stride,
                                                  zero stride broadcast, unique-byte deltas

  Mask vector memory      Complete                2 instructions (vlm.v, vsm.v),
                                                  EEW=8, EMUL=1, EVL=ceil(vl/8),
                                                  always tail-agnostic

  Indexed memory          Pending / optional      vluxei, vloxei, vsuxei, vsoxei

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
- Encodings: Standard RVV 1.0 realigned: OPIVV (`funct3 = 000`) for `.vv`, OPIVX (`funct3 = 100`) for `.vx`, OPIVI (`funct3 = 011`) for `.vi`.
- Scalar operands in `.vx` variants are masked/truncated to active SEW and interpreted with instruction-specific signedness.
- Immediate operands in `.vi` variants parse 5-bit signed immediate `simm5` in `[-16, 15]`, sign-extended to SEW bits.
- Deterministic mask tail policy: mask destination is always tail-agnostic (tail bits in $[vl, VLMAX)$ are set to 1 regardless of `vta`); bits beyond $VLMAX$ remain strictly undisturbed.
- Masking policy: inactive elements under `vm=0` remain undisturbed when `vma=0`, and are overwritten with 1 when `vma=1`.
- Hazard-free register overlap: source registers (`vs1`, `vs2`, `rs1`) and execution mask (`v0`) are snapshotted prior to destination updates, ensuring safe execution even when $vd = v0$, $vd = vs1$, or $vd = vs2$.
- Boundary semantics: $vstart \ge vl$ produces no body or tail modifications, leaves the destination register completely untouched, and resets `vstart` to 0 on writeback.
- Full delta tracking and undo/redo support.

### Vector mask logical operations

-   `vmand.mm` (bitwise AND)
-   `vmandn.mm` (bitwise AND with inverted source 2: `vs2 & ~vs1`)
-   `vmor.mm` (bitwise OR)
-   `vmorn.mm` (bitwise OR with inverted source 2: `vs2 | ~vs1`)
-   `vmxor.mm` (bitwise XOR)

Current implementation supports:
- All 5 mask logical instructions encoded with `funct3 = 010` (`OPMVV`) and `vm = 1`.
- `vm = 0` is reserved and rejected.
- Packed single-bit mask element operations independent of the configured SEW.
- Deterministic tail-agnostic policy: tail bits in $[vl, VLMAX)$ are set to 1; bits beyond $VLMAX$ remain undisturbed.
- Strict $vstart \ge vl$ handling: produces no body or tail modifications, leaves destination untouched, and resets `vstart` to 0 on writeback.
- Prestart bits in $[0, vstart)$ remain undisturbed when $0 < vstart < vl$.
- Full support for register aliasing ($vd = vs1$, $vd = vs2$, $vd = vs1 = vs2$, $vd = v0$) and delta undo/redo.

### Vector mask prefix operations

-   `vmsbf.m` (set bits before first active set bit)
-   `vmsof.m` (set only the first active set bit)
-   `vmsif.m` (set bits up to and including the first active set bit)

Current implementation supports:
- Shared encoding `funct6 = 010100`, `funct3 = 010` (`OPMVV`), distinguished by `vs1`: `vmsbf.m` (`vs1 = 1`), `vmsof.m` (`vs1 = 2`), `vmsif.m` (`vs1 = 3`).
- Both unmasked (`vmsbf.m vd, vs2`, `vm = 1`) and masked (`vmsbf.m vd, vs2, v0.t`, `vm = 0`) assembler syntax and machine-code generation.
- Correct masked prefix scan: inactive elements (`v0[i] == 0`) do not participate in or terminate the prefix scan; only active elements are evaluated to find the first set bit.
- Inactive elements follow the `vma` policy (retaining undisturbed bits when `vma=0`, setting bits to 1 when `vma=1`).
- Deterministic tail-agnostic policy applied when $vl > 0$: tail bits in $[vl, VLMAX)$ set to 1.
- Edge case $vstart == 0$ and $vl == 0$: destination completely untouched (no tail writes applied) and `vstart` reset to 0.
- Exception and hazard validations:
  - Nonzero `vstart` is illegal even when $vl = 0$ (early return without clearing `vstart`).
  - Destination/source overlap $vd == vs2$ is illegal.
  - Masked destination/mask overlap $vd == v0$ when `vm = 0` is illegal.
- Full delta undo/redo support.

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

### Strided vector memory

Loads: - `vlse8.v` - `vlse16.v` - `vlse32.v` - `vlse64.v`

Stores: - `vsse8.v` - `vsse16.v` - `vsse32.v` - `vsse64.v`

Current implementation supports:
- Strided loads and stores for EEW = 8, 16, 32, 64 bits.
- Address calculation: `element_address(i) = base + static_cast<uint64_t>(i) * static_cast<uint64_t>(stride)` using 64-bit unsigned modular arithmetic to cleanly support negative two's-complement strides without C++ undefined behavior.
- Base address from `rs1`, byte stride from scalar GPR `rs2` (with `rs2 = x0` yielding stride 0).
- Zero stride load acts as a broadcast of element 0 across all active vector elements; zero stride store writes elements sequentially ($0$ to $vl - 1$) with the highest active element value remaining in memory.
- Element overlap handling ($|stride| < EEW$): unique-address byte delta tracking (`std::map<uint64_t, uint8_t>`) records each modified byte exactly once per vector instruction, ensuring complete and correct undo/redo reversibility.
- Masked execution (`vm=0` and `vm=1`): inactive elements perform no memory reads or writes; mask-agnostic destination elements updated according to `vma`.
- `vstart` handling: resumes execution from element index `vstart`; $vstart \ge vl$ performs zero memory transfers, zero destination writes, and resets `vstart` to 0 on writeback.
- Fault/validation: instruction is rejected if `SEW != EEW`, `mop != 10`, `nf != 0`, or `mew != 0`.

### Mask vector memory

Loads: - `vlm.v`

Stores: - `vsm.v`

Current implementation supports:
- Mask load and store transferring packed mask elements as bytes with EEW = 8, EMUL = 1, independent of configured SEW.
- Effective vector length: $evl = \min(\lceil vl / 8 \rceil, 16)$ bytes.
- When $vl = 0$ ($evl = 0$), no memory accesses or register writes are performed, and `vstart` resets to 0.
- When $vl$ is not a multiple of 8, the entire partial final byte is transferred in full without masking out upper bits.
- Destination register tail policy: mask destination bytes $[evl, 16)$ are always tail-agnostic (set to `0xFF`), regardless of `vtype.vta`.
- Encoding validation: `mop = 00`, `lumop/sumop = 01011`, `funct3 = 000`, `vm = 1`; $vm = 0$ is reserved and rejected.
- `vstart` handling: $vstart$ is interpreted as a byte index; $vstart \ge evl$ performs zero memory transfers or register modifications, and resets `vstart` to 0.
- Full undo/redo fidelity with unique-address byte delta tracking.

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
5.  Current memory support includes unit-stride, strided, and mask load/store operations (single-field nf=0, mew=0, mop=00/10). Indexed memory is not yet supported.
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

### Priority B --- Vector mask logical and prefix operations (Implemented)

Implemented additions:
- Mask logical: `vmand.mm`, `vmandn.mm`, `vmor.mm`, `vmorn.mm`, `vmxor.mm`
- Mask prefix: `vmsbf.m`, `vmsof.m`, `vmsif.m`

These operate on packed single-bit mask elements independent of configured SEW. Tail bits in
$[vl, VLMAX)$ are set to 1 (deterministic tail-agnostic implementation choice), while bits beyond
$VLMAX$ remain strictly undisturbed. Prefix instructions support both unmasked and masked forms (`v0.t`),
correctly bypassing inactive elements during prefix scans. Nonzero `vstart` and illegal overlaps
($vd == vs2$, masked $vd == v0$) are rejected. When $vl=0$, destination is completely unchanged.

Deferred candidate mask logical extensions:
- `vmnand.mm`, `vmnor.mm`, `vmxnor.mm`, `vmnot.m`

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

### Priority E --- Additional memory addressing

Implemented additions:
- Strided loads: `vlse8.v`, `vlse16.v`, `vlse32.v`, `vlse64.v`
- Strided stores: `vsse8.v`, `vsse16.v`, `vsse32.v`, `vsse64.v`
- Mask memory: `vlm.v`, `vsm.v`

Pending optional memory candidates:
- Indexed unordered loads/stores (`vluxei*.v`, `vsuxei*.v`)
- Indexed ordered loads/stores (`vloxei*.v`, `vsoxei*.v`)

These require decoding additional vector-indexed memory formats and calculating per-element addresses based on vector index registers.

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

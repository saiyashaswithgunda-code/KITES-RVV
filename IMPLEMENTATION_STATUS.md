# RVV Support in KITES --- Implementation Status

## 1. Status summary

**Latest known verification:** 251/251 GoogleTests passed after Phase 6.

The statuses below describe the implementation at the handoff point. A
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

  Vector-vector integer   Complete                Five operations listed
  arithmetic                                      below

  Unit-stride vector      Complete for current    Eight load/store
  memory                  subset                  variants

  Masked execution        Complete for            `vm=0` and `vm=1`
                          implemented operations  

  Tail/mask policy        Complete for            Deterministic agnostic
  behavior                implemented operations  behavior

  Undo/redo               Complete for covered    Verify for each new
                          vector state changes    instruction

  Additional arithmetic   Pending                 See Section 4
  families                                        

  Vector comparisons      Pending                 See Section 4

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

Current implementation supports: - SEW = 8, 16, 32, 64. - LMUL = m1. -
Configured `vl` and `vstart`. - Masked and unmasked execution. - Tail
and mask policy behavior. - Source/destination overlap handled by
snapshotting source values before destination updates.

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
4.  Current vector arithmetic supports only the five vector-vector
    operations listed above.
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

### Priority A --- Vector-scalar arithmetic

Suggested first additions: - `vadd.vx` - `vsub.vx`

Why first: these reuse the existing vector arithmetic execution model,
but require correct scalar operand extraction, sign extension, and
element-width behavior.

Questions to resolve: - Is the scalar operand treated as XLEN-wide and
then sign-extended/truncated according to the instruction's
architectural rules? - What happens when SEW is narrower than XLEN? -
What happens for negative scalar values? - What operand overlap
restrictions apply?

### Priority B --- Vector-immediate arithmetic

Possible additions: - `vadd.vi` - `vrsub.vi` (verify exact mnemonic and
semantics; do not assume it is simply `vsub.vi`)

Important: RVV immediate operands have specific sign-extension and
encoding rules. Do not treat the immediate as an arbitrary unsigned
5-bit number.

### Priority C --- Vector comparisons

Possible additions include: - `vmseq.vv` - `vmsne.vv` - `vmslt.vv`

Comparison instructions generally write predicate bits to a vector mask
destination. Their destination semantics differ from ordinary vector
arithmetic: result bits are packed into mask elements rather than
written as one full SEW-wide value per element. Carefully inspect
mask-register representation and `v0` handling before implementation.

### Priority D --- Vector mask logical operations

Potential family: - `vmand.mm` - `vmor.mm` - `vmxor.mm` - `vmnot.m`

These operate on mask bits and require dedicated handling; do not route
them through ordinary SEW arithmetic without checking the specification.

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

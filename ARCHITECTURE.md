# RVV Support in KITES --- Architecture and Integration Notes

## 1. Architectural overview

The implementation adds vector state to the existing KITES
`RegisterFile` and adds vector instruction paths to the RVSS processor.
The assembler remains responsible for translating vector assembly into
machine code.

Conceptual flow:

``` text
Assembly source
    |
    v
Lexer -> Parser -> ICUnit / intermediate code
    |
    v
Code generator -> 32-bit machine instruction
    |
    v
RVSS Fetch -> Decode -> Execute -> Memory -> WriteBack
    |                                      |
    |                                      v
    |                                RegisterFile
    |                                Memory controller
    |
    v
Undo/redo delta tracking
```

## 2. Vector register file

Relevant files: - `src/processor/registers.h` -
`src/processor/registers.cpp` - `test/vector_register_test.cpp`

The register file defines: - `NUM_VREG = 32` - `VLEN_BITS = 128` -
`VLEN_BYTES = 16` - `VectorRegister = std::array<uint8_t, VLEN_BYTES>`

The vector register storage is a 32-entry array. The register file
exposes vector read/write methods and typed element accessors for
supported element widths.

Important implementation properties: - Elements are represented as
bytes. - Typed element access follows little-endian byte ordering. -
Element index and width bounds are checked. - `Reset()` clears vector
register contents. - Do not assume the register file is a `uint64_t[]`;
it stores raw bytes to support all current SEW values.

For LMUL=m1, the number of elements that fit in one vector register is:

    SEW   Elements per 128-bit register
  ----- -------------------------------
      8                              16
     16                               8
     32                               4
     64                               2

## 3. Vector CSRs

Defined in `registers.h`:

``` cpp
static constexpr size_t CSR_VSTART = 0x008;
static constexpr size_t CSR_VL     = 0xC20;
static constexpr size_t CSR_VTYPE  = 0xC21;
static constexpr size_t CSR_VLENB  = 0xC22;
```

Reset state in `RegisterFile::Reset()` includes: - `vstart = 0` -
`vl = 0` - `vtype = 0` - `vlenb = VLEN_BYTES` (16)

The generic `ReadCsr` and `WriteCsr` interface is used. Do not assume
all architectural CSR privilege, read-only, or trap rules have been
implemented. In particular, inspect the existing CSR/trap infrastructure
before changing `vlenb` write behavior or adding new CSR semantics.

## 4. VTYPE decoder and vector length

Relevant files: - `src/processor/vector_config.h` -
`src/processor/vector_config.cpp` - `test/vector_config_test.cpp`

The `VectorConfig` structure contains: - `sew_bits` - `lmul` -
`tail_agnostic` - `mask_agnostic` - `vill` - `vlmax`

The decoder interprets: - bit 63: `vill` - bits 62:8: reserved - bit 7:
`vma` - bit 6: `vta` - bits 5:3: `vsew` - bits 2:0: `vlmul`

Current accepted configuration: - LMUL=m1 only. - SEW 8, 16, 32, or
64. - `VLMAX = VLEN_BITS / SEW`.

Unsupported/reserved combinations produce `vill=true`. The
implementation uses a deterministic `CalculateVectorLength(avl, vlmax)`
that returns `min(AVL, VLMAX)`.

## 5. `vsetvli` execution

Relevant processor code: - `src/processor/rvss/rvss_processor.h` -
`src/processor/rvss/rvss_processor.cpp`

The instruction is identified by: - opcode `0x57` - funct3 `0b111` - bit
31 clear for the current `vsetvli` form

The implementation extracts `rd`, `rs1`, and the 11-bit VTYPE immediate.

AVL handling: - `rs1=x0`, `rd!=x0`: AVL is treated as effectively
unlimited (`UINT64_MAX`), so VL becomes VLMAX. - `rs1=x0`, `rd=x0`:
preserve current VL as AVL. - Otherwise: AVL comes from GPR `rs1`.

For a valid VTYPE: - update `vtype` - calculate and update `vl` - reset
`vstart` to zero - write VL to `rd`, unless `rd=x0`

For invalid VTYPE: - set `vill` - set `vl=0`

The operation records changed CSR/GPR values in the processor's current
undo delta.

## 6. Vector arithmetic execution

Relevant files: - `src/processor/rvss/rvss_processor.cpp` -
`src/processor/rvss/rvss_processor.h` -
`src/processor/processor_base.h` - `test/vector_arithmetic_test.cpp`

Implemented operations: - `vadd.vv` - `vsub.vv` - `vand.vv` - `vor.vv` -
`vxor.vv`

Execution flow: 1. Decode opcode/funct fields and register indices. 2.
Decode current VTYPE and validate it. 3. Determine SEW and maximum
elements for the current vector register. 4. Read `vl`, `vstart`, `vm`,
`vta`, and `vma`. 5. Snapshot source registers and the mask register
before writing the destination. This makes supported overlap cases
deterministic. 6. Process active elements from `vstart` to `vl`. 7. For
masked-off elements, preserve the destination when mask-undisturbed is
selected; otherwise use the implementation's deterministic all-ones
value for agnostic elements. 8. For tail elements, preserve or set all
ones according to tail policy. 9. Write the resulting destination and
reset `vstart`. 10. Record vector-register and CSR changes for
undo/redo.

Prestart elements (`0` through `vstart-1`) remain unchanged.

Agnostic values are architecturally unconstrained. This implementation
chooses all ones (`0xFF` in each byte) as a deterministic simulator
behavior. Keep tests consistent with this documented choice, but do not
describe it as the only architecturally valid agnostic result.

## 7. Vector memory execution

Relevant files: - `src/processor/rvss/rvss_processor.cpp` -
`src/processor/rvss/rvss_processor.h` - `test/vector_memory_test.cpp`

Implemented: - `vle8.v`, `vle16.v`, `vle32.v`, `vle64.v` - `vse8.v`,
`vse16.v`, `vse32.v`, `vse64.v`

Current encoding subset: - opcode `0x07` for loads. - opcode `0x27` for
stores. - `nf=0`, `mew=0`, `mop=0`. - `lumop/sumop=0` for unit-stride. -
`vm` can be 0 or 1. - `funct3` selects element width: 000=8-bit,
101=16-bit, 110=32-bit, 111=64-bit.

The execution path checks: - supported unit-stride encoding fields. -
valid VTYPE. - SEW matches memory element width. - `vl` and `vstart` fit
the vector register's element capacity.

The current implementation stores pending memory-operation metadata in
RVSS processor fields. `ExecuteVectorMemory()` snapshots required state.
`WriteMemoryVector()` performs element-wise reads/writes.
`WriteBackVectorMemory()` writes a loaded vector destination, resets
`vstart`, and records vector/CSR changes.

Memory controller interface used by the implementation:

``` cpp
void writeByte(uint64_t address, uint8_t value);
void writeHalfWord(uint64_t address, uint16_t value);
void writeWord(uint64_t address, uint32_t value);
void writeDoubleWord(uint64_t address, uint64_t value);

uint8_t readByte(uint64_t address);
uint16_t readHalfWord(uint64_t address);
uint32_t readWord(uint64_t address);
uint64_t readDoubleWord(uint64_t address);
uint8_t peekByte(uint64_t address);
```

The current vector memory implementation uses byte operations to build
element values and record old/new memory bytes. Stores record memory
changes only when the byte value changes. Masked-off stores do not
modify memory. Tail elements are not stored.

**Caution:** this is the project's current subset implementation, not a
complete implementation of all RVV memory fault, alignment, ordering,
segment, stride, or indexed-addressing rules.

## 8. Mask and tail semantics

The current implementation supports: - `vm=1`: unmasked. - `vm=0`:
masked using bit `i` of `v0` for element `i`. - `vma=0`:
mask-undisturbed behavior for masked-off destination elements. -
`vma=1`: masked-off destination elements are set to all ones as the
project's deterministic agnostic choice. - `vta=0`: tail-undisturbed
behavior. - `vta=1`: tail elements are set to all ones as the project's
deterministic agnostic choice.

For stores: - Masked-off elements do not write memory. - Tail elements
are not stored. - `vma` does not cause masked-off store locations to be
modified.

For all currently implemented vector instructions: - Elements before
`vstart` are preserved. - A successful vector operation resets `vstart`
to zero. - The mask register is snapshotted before execution so a
destination update cannot retroactively change the mask used for the
current instruction.

## 9. Undo/redo integration

Relevant files: - `src/processor/processor_base.h` -
`src/processor/rvss/rvss_processor.cpp` -
`src/processor/rvss/rvss_processor.h`

The processor records changes in its current step delta. Existing change
categories include: - scalar register changes - CSR changes -
floating-point register changes - vector register changes - memory
changes

`RegisterChange` uses a register type discriminator: - `0`: GPR - `1`:
CSR - `2`: FPR

Vector register changes are tracked separately. Memory changes retain
old and new byte sequences so undo and redo can restore memory.

When adding a new instruction: - Record the old and new value
before/after modification. - Avoid recording unchanged state unless
existing conventions require it. - Ensure a single instruction can be
undone and redone exactly. - If the instruction changes multiple vector
registers, record every changed register. - If it changes a CSR such as
`vstart`, record that CSR too. - If it writes memory, record old and new
bytes for every changed address.

## 10. Assembler integration points

Relevant files include: - `src/assembler/instructions.h` -
`src/assembler/instructions.cpp` - `src/assembler/lexer.cpp` -
`src/assembler/parser.h` - `src/assembler/parse.cpp` -
`src/assembler/parse_formats/i_m_formats.cpp` -
`src/assembler/code_generator.cpp`

General extension pattern: 1. Add an `Instruction` enum value. 2. Add
mnemonic-to-enum mapping. 3. Add mnemonic to valid instruction
collection. 4. Add syntax mapping. 5. Add machine encoding metadata if
the existing encoding framework supports the format. 6. Add a parser
method for a new operand form, if required. 7. Add parser dispatch in
`parseTextDirective()`. 8. Add code-generation dispatch before generic
scalar format paths if vector instructions need custom encoding. 9. Add
intermediate-code pretty-print handling if the current printer assumes
scalar operands. 10. Add parser and exact machine encoding tests.

Existing vector syntax types include: - `O_GPR_C_GPR_C_VTYPE` for
`vsetvli`. - `O_VREG_C_VREG_C_VREG` for vector-vector arithmetic. -
`O_VREG_C_LP_GPR_RP` for vector memory operands (`vd/vs3, (base)`).

Vector registers are lexed as `VEC_REGISTER` for `v0` through `v31`.

## 11. Suggested code organization for new operations

Prefer dedicated helper methods or clearly separated dispatch
functions: - `ExecuteVectorArithmetic()` for vector arithmetic. -
`ExecuteVectorMemory()` for vector memory instruction setup. -
`WriteMemoryVector()` for vector memory element transfer. -
`WriteBackVectorMemory()` for vector load writeback. - Existing
`ExecuteVsetvli()` / `WriteBackVsetvli()` for configuration.

If new arithmetic formats are introduced, consider either: - extending
`ExecuteVectorArithmetic()` with a clearly defined operation class and
operand source, or - adding a dedicated helper if the semantics differ
substantially.

Do not force comparisons or mask logical operations into ordinary
arithmetic if their destination layout is different.

## 12. Before modifying code

Read the current definitions and call flow rather than relying only on
this document. In particular inspect: - `RVSSProcessor::Execute()`,
`WriteMemory()`, `WriteBack()`, `Undo()`, `Redo()`, `Reset()`. -
`RegisterFile` vector and CSR accessors. - `VectorConfig` decoder. -
assembler instruction maps, parser dispatch, and code generator. -
current vector tests.

The source code is authoritative if it differs from this document.

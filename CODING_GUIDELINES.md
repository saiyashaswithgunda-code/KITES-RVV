# RVV Support in KITES --- Coding Guidelines

## 1. General principles

1.  Follow the existing KITES style and naming conventions in the file
    being edited.
2.  Keep changes scoped to one instruction family or one coherent
    feature.
3.  Reuse the existing assembler, processor, register-file, memory, and
    undo/redo abstractions.
4.  Do not duplicate architectural state in multiple places unless the
    processor pipeline requires a temporary snapshot.
5.  Do not silently reinterpret unsupported encodings as a different
    instruction.
6.  Verify instruction encodings and semantics against the RVV 1.0
    specification.
7.  Prefer readable, explicit code over compressed bit-manipulation
    expressions.

## 2. Naming and organization

-   Follow existing method naming in the relevant class.
-   Existing vector-related names include `ReadVreg`, `WriteVreg`,
    `ExecuteVsetvli`, `WriteBackVsetvli`, `ExecuteVectorArithmetic`,
    `ExecuteVectorMemory`, `WriteMemoryVector`, and
    `WriteBackVectorMemory`.
-   Keep declarations in the corresponding header and definitions in the
    corresponding source file.
-   Keep test files feature-oriented, e.g. `vector_arithmetic_test.cpp`,
    `vector_memory_test.cpp`, and `vector_assembler_test.cpp`.
-   Use descriptive test names that state the architectural behavior
    being checked.

## 3. Register and element handling

-   Vector registers are 16-byte arrays.
-   Do not assume host endianness when assembling/disassembling
    elements; follow the existing little-endian element access helpers.
-   Use the existing typed accessors when appropriate.
-   Check element index and width boundaries.
-   For SEW operations, use the decoded SEW rather than hard-coding a
    width.
-   Be careful with shifts by the full integer width; avoid undefined
    behavior when constructing masks for 64-bit elements.
-   Preserve bytes outside the elements that the instruction is
    architecturally allowed to modify.

## 4. Instruction decode and execution

-   Decode opcode, funct3, funct6, vm, and register fields explicitly.
-   Separate instruction validation from element execution.
-   Validate current VTYPE and supported SEW/LMUL before using vector
    state.
-   Respect `vl`, `vstart`, mask, tail policy, and mask policy where
    relevant.
-   Do not assume every vector instruction has the same destination
    format.
-   Comparisons and mask logical instructions write packed mask bits and
    should use a mask-aware implementation.
-   Vector memory instructions have additional encoding fields (`nf`,
    `mew`, `mop`, `vm`, `lumop/sumop`, `mop`, `mew`, etc.). Validate all
    fields required by the supported subset.
-   For unsupported encodings, follow the simulator's established
    illegal-instruction/error convention. The current vector paths have
    some early-return behavior; improve this only after inspecting the
    processor's exception mechanism and preserving consistency.

## 5. Mask and tail policy

For the current implementation: - Mask bit for element `i` is bit
`(i % 8)` of byte `(i / 8)` in `v0`. - `vm=1` means unmasked. - `vm=0`
means use `v0` as the predicate. - `vma=0` preserves masked-off
destination elements. - `vma=1` uses all ones as the deterministic
agnostic result. - `vta=0` preserves tail elements. - `vta=1` uses all
ones as the deterministic agnostic result. - Prestart elements before
`vstart` remain unchanged. - Stores skip masked-off and tail elements.

The all-ones value is a deterministic project choice for architecturally
agnostic elements, not a unique required value.

## 6. Source/destination overlap

-   Snapshot source vector registers before modifying the destination if
    overlap is allowed.
-   Snapshot `v0` before execution when it is used as a mask.
-   Check RVV overlap restrictions for each new instruction. Some
    instructions impose special restrictions involving mask destinations
    or source/destination overlap.
-   Add tests for legal overlap and for prohibited overlap behavior if
    the simulator models illegal instruction constraints.

## 7. Undo/redo

Every instruction must integrate with the existing step-delta mechanism.

-   Record old and new vector register contents for every modified
    destination.
-   Record CSR changes, especially `vstart` reset.
-   Record old and new memory bytes for stores.
-   Do not mutate memory without recording the prior contents.
-   Test undo and redo independently.
-   Confirm undo/redo works when the destination overlaps a source
    register.
-   Follow the existing `RegisterChange` discriminator convention:
    GPR=0, CSR=1, FPR=2. Vector register changes use their dedicated
    vector-change collection.

## 8. Assembler and code generator

When adding an instruction, inspect all of: - `Instruction` enum. -
`instruction_string_map`. - `valid_instructions`. -
`instruction_syntax_map`. - `instruction_encoding_map`. - Lexer
tokenization. - Parser declarations and syntax dispatch. -
Intermediate-code field conventions. - Code-generator dispatch and
instruction printer.

Do not assume the generic scalar instruction encoder can encode vector
instructions correctly. Add a dedicated encoder when the field layout
differs.

For every new encoder, add tests with known expected 32-bit words.
Derive expected words independently from the ISA field layout; do not
copy the implementation's expression into the test without independent
checking.

## 9. Error handling and unsupported forms

-   Inspect existing KITES behavior for invalid instructions, traps, and
    memory errors before choosing a response.
-   Do not invent a new error-reporting mechanism.
-   Avoid silently accepting reserved fields.
-   If only a subset is supported, explicitly validate and reject fields
    outside that subset.
-   Document any temporary limitation.
-   Do not broaden scope by adding partial trap behavior without tests.

## 10. Git and collaboration

-   Work on a dedicated branch for each feature or contributor.
-   Avoid committing local build paths, generated build files, binaries,
    or IDE settings.
-   In the original development environment, `CMakePresets.json`
    contains local machine-specific modifications and was intentionally
    excluded from commits. Check `git status` before staging.
-   Stage explicit paths rather than using `git add .` when local
    configuration files may be modified.
-   Use focused commits with descriptive messages.
-   Push the branch and open a PR or share the commit hash with the
    project owner.

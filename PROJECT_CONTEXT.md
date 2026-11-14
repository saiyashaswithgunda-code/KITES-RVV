# RVV Support in KITES --- Project Context

## 1. Purpose

This project extends the existing KITES RISC-V simulator with a
carefully scoped subset of the RISC-V Vector Extension (RVV 1.0). The
target is the **RVSS single-stage processor**. The work is intended as a
Computer Architecture course project and should remain compatible with
the existing KITES architecture, assembler, execution model, tests, and
undo/redo functionality.

The immediate handoff goal is to let another contributor understand the
existing implementation and continue adding the remaining vector
instructions without accidentally breaking completed functionality.

## 2. Repository and working environment

-   Repository: `saiyashaswith-gunda-code/KITES-RVV`
-   Working branch: `rvv-development`
-   Development environment used: Windows, CMake, Ninja, MinGW/GCC, Qt.
-   The local build configuration file `CMakePresets.json` contains
    machine-specific paths. It was modified locally to point to the
    developer's Qt and MinGW installations and was intentionally
    excluded from project commits. **Do not blindly commit this file or
    overwrite another developer's local paths.**
-   The project uses GoogleTest.
-   The implementation was developed incrementally in phases, with tests
    added for each phase.

Typical build and test commands from the repository root:

``` powershell
cmake --build --preset test-only
ctest --preset test-only --output-on-failure
```

If the preset is not configured on a new machine, inspect
`CMakePresets.json` and the repository's CMake configuration first. Do
not copy absolute paths from another developer's machine.

## 3. Existing KITES architecture: relevant concepts

The existing simulator separates responsibilities across the assembler
and processor:

1.  **Lexer/parser** convert assembly source into intermediate code
    (`ICUnit`).
2.  **Code generator** converts intermediate code into 32-bit machine
    instructions.
3.  **RVSS processor** fetches, decodes, executes, accesses memory, and
    writes results back.
4.  **RegisterFile** owns scalar and vector architectural state.
5.  **Memory controller** provides byte/halfword/word/doubleword access.
6.  **Undo/redo** records architectural changes made during a step.
7.  **GoogleTest** validates isolated components and end-to-end
    instruction behavior.

When extending RVV, preserve this separation. Avoid implementing
assembly parsing inside the processor or encoding logic inside execution
functions.

## 4. RVV design decisions already made

The current implementation uses these deliberate project-level choices:

  -----------------------------------------------------------------------
  Item                                Current choice
  ----------------------------------- -----------------------------------
  ISA target                          RVV 1.0 subset

  Processor                           RVSS single-stage processor

  VLEN                                128 bits

  VLENB                               16 bytes

  Vector registers                    32 registers, `v0`--`v31`

  Initial LMUL support                `m1` only

  Supported SEW                       8, 16, 32, 64 bits

  Vector memory                       Unit-stride, single-field subset

  Mask register                       `v0`

  Execution state                     `vl`, `vtype`, `vstart`, `vlenb`

  Required quality                    Assembler + machine encoding +
                                      execution + tests + undo/redo where
                                      state changes
  -----------------------------------------------------------------------

The simulator's fixed VLEN is a project configuration, not a claim that
RVV hardware must use 128-bit vectors.

## 5. Completed implementation at handoff

The following feature groups are implemented and have passed the
complete test suite:

-   Vector register storage and typed element access.
-   Vector CSRs and `vsetvli` execution.
-   Assembler and encoding for `vsetvli`.
-   Vector-vector arithmetic: `vadd.vv`, `vsub.vv`, `vand.vv`, `vor.vv`,
    `vxor.vv`.
-   Unit-stride vector loads and stores: `vle8/16/32/64.v`,
    `vse8/16/32/64.v`.
-   Masked and unmasked arithmetic and memory behavior.
-   Tail and mask agnostic/undisturbed behavior as described in
    `ARCHITECTURE.md`.
-   Undo/redo tracking for vector register, CSR, and memory changes
    covered by the implemented instructions.

Latest known full-suite result at the handoff point: **251/251 tests
passed**.

## 6. Git history milestones

Known milestone commits on `rvv-development`:

-   `97e9778` --- Implement RVV vector register file
-   `56e6b19` --- Implement RVV vector configuration and vsetvli
-   `b88f600` --- Implement RVV vector instruction assembly and encoding
-   `f3db17b` --- Implement RVV vector memory instructions
-   `99ddfa0` --- Implement RVV mask and tail semantics

The vector arithmetic execution phase was also committed and pushed; its
exact commit hash is not recorded in this handoff document. Use
`git log --oneline --decorate` to inspect the complete history.

## 7. Recommended continuation strategy

Continue in small, reviewable feature increments:

1.  Inspect the relevant existing code and instruction metadata.
2.  Confirm the exact RVV encoding and architectural semantics from the
    RVV 1.0 specification.
3.  Add instruction metadata and assembler support.
4.  Add code generation and exact encoding tests.
5.  Add execution support in RVSS.
6.  Add tests for SEW, VL, VSTART, masks, overlap, and boundary
    behavior.
7.  Add undo/redo tests if the instruction changes architectural state.
8.  Run the full suite.
9.  Commit only the intended source and test files.

Do not add multiple unrelated instruction families in one change.
Preserve all previously passing tests.

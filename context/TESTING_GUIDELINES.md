# RVV Support in KITES --- Testing Guidelines

## 1. Baseline verification

Before editing, build and run the complete test suite:

``` powershell
cmake --build --preset test-only
ctest --preset test-only --output-on-failure
```

Latest known baseline at handoff: **251/251 tests passed**.

If the test count differs, inspect the output and current branch before
assuming a regression. Do not proceed with a failing baseline unless the
failure is understood and documented.

## 2. Existing vector test files

Known vector test files:

  ------------------------------------------------------------------------
  Test file                            Responsibility
  ------------------------------------ -----------------------------------
  `test/vector_register_test.cpp`      Vector register storage and element
                                       access

  `test/vector_csr_test.cpp`           Vector CSR initialization and
                                       access

  `test/vector_config_test.cpp`        VTYPE decoding and VL calculation

  `test/vector_instruction_test.cpp`   `vsetvli` execution, state changes,
                                       undo/redo

  `test/vector_assembler_test.cpp`     Vector assembly parsing and machine
                                       encodings

  `test/vector_arithmetic_test.cpp`    Vector arithmetic execution, masks,
                                       tails, VSTART, undo/redo

  `test/vector_memory_test.cpp`        Vector loads/stores, masks, tails,
                                       VSTART, undo/redo
  ------------------------------------------------------------------------

Check the actual repository before relying on this list; tests may have
been reorganized.

## 3. Test layers

### A. Register and configuration tests

Test: - Reset values. - All 32 vector registers. - Typed reads/writes
for 8/16/32/64-bit elements. - Little-endian behavior. - Out-of-range
element access. - Valid and invalid VTYPE encodings. - VLMAX for each
supported SEW. - AVL below, equal to, and above VLMAX.

### B. Assembler tests

For every new instruction: - Valid mnemonic recognized. - Correct
operand count and token types. - Correct register parsing, including
boundary registers (`v0`, `v31`, `x0`, `x31`). - Invalid register names
rejected. - Correct intermediate-code fields. - Exact 32-bit machine
encoding. - Multiple instructions assemble in one source program. -
Intermediate-code printing does not misrepresent operands.

Use known instruction words derived independently from the ISA encoding
table.

### C. Execution tests

For each instruction: - Basic operation with known inputs. - All
supported SEW values. - Multiple vector elements. - `vl=0`, `vl=1`, and
full VLMAX. - Nonzero `vstart`. - `vstart=vl`. - Masked and unmasked
operation where applicable. - Both mask policies and tail policies where
applicable. - Prestart, masked-off, and tail elements retain or change
exactly as expected. - Legal source/destination overlap. - Invalid VTYPE
and unsupported instruction encoding behavior.

### D. Memory tests

For vector loads/stores: - All supported element widths. - Correct base
address and per-element address. - `vl` limits the number of transferred
elements. - `vstart` skips prestart elements. - Masked-off stores leave
memory unchanged. - Tail elements are not stored. - Loads preserve
inactive destination elements when required. - Memory boundaries and
alignment behavior follow the existing simulator contract. - Undo/redo
restores exact bytes.

### E. Undo/redo tests

For every instruction that changes state: 1. Initialize all relevant
registers/CSRs/memory. 2. Execute one instruction. 3. Verify the new
state. 4. Undo. 5. Verify every modified location matches its original
value. 6. Redo. 7. Verify every modified location matches the
post-execution value.

Include vector destination, `vstart`, other changed CSRs, and memory
where relevant.

## 4. Test helper discipline

Existing tests use helper functions to encode instructions and
initialize vector state. Reuse helpers when appropriate, but keep
helpers explicit and avoid making test expectations depend on
implementation internals.

For example, the current `vsetvli` encoding helper uses:

``` cpp
(vtype << 20) | (rs1 << 15) | (7 << 12) | (rd << 7) | 0x57
```

The vector-vector arithmetic helper encodes the relevant funct6, vm,
vs2, vs1, funct3, vd, and opcode fields. Inspect the actual test helper
before reusing it.

## 5. Running focused tests

The test executable is commonly available at:

``` powershell
.\build\test\kites_test.exe
```

GoogleTest supports filters. Examples:

``` powershell
.\build\test\kites_test.exe --gtest_filter="VectorArithmeticTest.*"
.\build\test\kites_test.exe --gtest_filter="VectorMemoryTest.*"
.\build\test\kites_test.exe --gtest_filter="VectorAssemblerTest.*"
```

Use the actual suite names from the source if they differ. To list test
names:

``` powershell
.\build\test\kites_test.exe --gtest_list_tests
```

After focused tests pass, always run the complete suite:

``` powershell
ctest --preset test-only --output-on-failure
```

## 6. Regression and completion checklist

Before committing a feature, verify:

-   [ ] Existing full suite passed before work.
-   [ ] New parser tests added.
-   [ ] Exact machine encoding tests added.
-   [ ] Execution tests added.
-   [ ] SEW/VL/VSTART cases covered where applicable.
-   [ ] Mask and tail behavior covered where applicable.
-   [ ] Overlap behavior checked.
-   [ ] Undo/redo covered for all modified state.
-   [ ] Unsupported encodings rejected consistently.
-   [ ] Full suite passes.
-   [ ] Documentation and `INSTRUCTION_TRACKER.md` updated.
-   [ ] Only intended files staged.
-   [ ] No local build configuration or generated artifacts committed.

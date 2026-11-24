# Benchmark: Integer Matrix Multiplication (`matrix_multiplication`)

## 1. Overview & Application Domain
Matrix multiplication is a cornerstone workload in numerical analysis, scientific computing, computer graphics transformations, and machine learning (dense GEMM layers).

## 2. Mathematical Definition
For two square matrices $\mathbf{A}, \mathbf{B} \in \mathbb{Z}^{N \times N}$, the product $\mathbf{C} = \mathbf{A} \times \mathbf{B}$ is defined by:
$$C[i][j] = \sum_{k=0}^{N-1} A[i][k] \cdot B[k][j]$$

Matrices are represented as contiguous 1D arrays in **row-major layout**:
$$\text{index}(i, j) = i \cdot N + j$$

The reference implementation uses an $i$-$k$-$j$ loop interchange:
```c
for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t k = 0; k < n; ++k) {
        int32_t aik = a[i * n + k];
        for (uint32_t j = 0; j < n; ++j) {
            c[i * n + j] += aik * b[k * n + j];
        }
    }
}
```
This loop ordering allows row $k$ of $\mathbf{B}$ and row $i$ of $\mathbf{C}$ to be accessed contiguously with unit stride, while $A[i][k]$ acts as a scalar multiplier. In RVV, this maps directly to unit-stride vector loads (`vle32.v`), vector multiply-accumulate with a scalar operand (`vmacc.vx`), and unit-stride vector stores (`vse32.v`), avoiding non-unit stride column accesses.

## 3. Data Representation & Semantics
- **Element Type:** `int32_t` (32-bit signed two's complement integer).
- **Accumulator Type:** `int32_t` (32-bit signed accumulator).
- **Arithmetic & Overflow Semantics:** Standard C signed integer arithmetic. Input elements lie in $[-14, 14]$ for $\mathbf{A}$ and $[-15, 15]$ for $\mathbf{B}$. Each product term satisfies $|A_{ik} \cdot B_{kj}| \le 210$. For $N=16$, the maximum accumulated inner product cannot exceed $16 \times 210 = 3360 \ll 2^{31}-1$, mathematically guaranteeing that no signed integer overflow can occur.
- **Problem Size:** $N = 16$ ($16 \times 16$ matrices, $256$ elements each; $1024$ bytes per matrix, $3072$ bytes total).

## 4. Input Dataset & Expected Output
- **Input Data:** Deterministic pseudo-random matrices $\mathbf{A}$ and $\mathbf{B}$ defined in [`inputs/input_data.h`](inputs/input_data.h).
- **Expected Checksum:**
  $$\text{Checksum} = \sum_{i=0}^{15} \sum_{j=0}^{15} C[i][j] = -78$$
- Complete precomputed golden matrix $\mathbf{C}$ is provided in [`inputs/input_data.h`](inputs/input_data.h).

## 5. Host Compilation & Verification
To compile and execute the canonical C reference on the host machine:
```bash
gcc -O3 -Wall -Wextra benchmarks/matrix_multiplication/reference/matrix_multiplication.c -o /tmp/matmul_host
/tmp/matmul_host
```
The host runner verifies every single element of the $16 \times 16$ product against the precomputed golden reference and asserts the overall matrix checksum.

## 6. Bare-Metal RISC-V Compilation
The C reference implementation is compiled into standalone bare-metal RISC-V assembly using `riscv64-elf-gcc`:

```bash
# 1. Scalar RV32IM (-O3)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32im -mabi=ilp32 -O3 -S \
    benchmarks/matrix_multiplication/reference/matrix_multiplication.c \
    -o benchmarks/matrix_multiplication/compiler_output/matrix_multiplication_scalar_gcc.s

# 2. Vector RV32IMV (-O3, LMUL=m1)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32imv -mabi=ilp32 -O3 -mrvv-max-lmul=m1 -S \
    -fopt-info-vec-all=benchmarks/matrix_multiplication/compiler_output/vectorization_report.txt \
    benchmarks/matrix_multiplication/reference/matrix_multiplication.c \
    -o benchmarks/matrix_multiplication/compiler_output/matrix_multiplication_vector_gcc.s
```

## 7. KITES Simulation Assembly
Standalone, self-contained KITES assembly files are provided in [`kites_assembly/`](kites_assembly/):
- **Scalar Assembly:** [`matrix_multiplication_scalar_kites.s`](kites_assembly/matrix_multiplication_scalar_kites.s)
- **Vector Assembly:** [`matrix_multiplication_vector_kites.s`](kites_assembly/matrix_multiplication_vector_kites.s)

### Vector Kernel Adaptation Details:
1. **Matrix Output Zeroing:** Replaced compiler-generated fractional LMUL (`e8, mf4`) zeroing with native 32-bit vector zeroing using `vxor.vv v1, v1, v1` and `vse32.v` with `LMUL=m1` (zeroing 256 words in only 192 dynamic instructions).
2. **Scalar-Vector Multiply-Accumulate:** In the $i$-$k$-$j$ loop order, $A[i \times N + k]$ is a scalar multiplier applied across row vector $B[k, :]$ into row vector $C[i, :]$. Rather than emulating scalar broadcasts via unsupported `vmv.v.x` and `vmadd.vv`, the kernel directly uses the newly implemented **`vmacc.vx vd, rs1, vs2`** instruction, streaming 4-element row slices with contiguous `vle32.v` and `vse32.v`.
3. **Syntax Normalization:** Stripped leading dots from branch labels, normalized unit-stride vector memory addressing syntax from `0(rs1)` to `(rs1)`, and removed unsupported directives.

### Execution Commands:
```bash
# Run Scalar Matrix Multiplication on KITES
build/cli/kites-cli benchmarks/matrix_multiplication/kites_assembly/matrix_multiplication_scalar_kites.s

# Run Vector Matrix Multiplication on KITES (with full memory dump of output matrix C)
build/cli/kites-cli benchmarks/matrix_multiplication/kites_assembly/matrix_multiplication_vector_kites.s --mem-dump 0x10000800 128
```

## 8. Simulation Results & Verification

Simulation metrics extracted from hardware execution steps and recorded in [`results/execution_summary.json`](results/execution_summary.json):

| Metric | Scalar (RV32IM) | Vector (RV32IMV) | Reduction / Speedup |
| :--- | :---: | :---: | :---: |
| **Kernel Dynamic Instructions** | 29,772 | 12,632 | **57.57% reduction ($2.36\times$ speedup)** |
| **Total Dynamic Instructions** | 31,066 | 13,926 | **55.17% reduction ($2.23\times$ speedup)** |
| **Harness Overhead Instructions** | 1,294 | 1,294 | Exact match |
| **Returned Checksum (`x10` / `a0`)** | `-78` (`0xFFFFFFB2`) | `-78` (`0xFFFFFFB2`) | **100% golden match** |
| **Element Mismatches** | 0 / 256 | 0 / 256 | **100% bit-for-bit match** |
| **Simulator Errors** | 0 | 0 | Clean execution |


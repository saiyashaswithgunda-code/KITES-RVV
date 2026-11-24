# Benchmark: Integer Dot Product (`dot_product`)

## 1. Overview & Application Domain
The integer dot product is a fundamental linear algebra and digital signal processing (DSP) kernel. It represents the inner product of two vectors and serves as the core computational primitive in matrix multiplication, digital filtering (FIR/IIR), neural network forward passes, and graphic projection calculations.

## 2. Mathematical Definition
Given two integer vectors $\mathbf{a}, \mathbf{b} \in \mathbb{Z}^N$:
$$\text{dot\_product} = \sum_{i=0}^{N-1} a[i] \cdot b[i]$$

## 3. Data Representation & Semantics
- **Element Type:** `int32_t` (32-bit signed two's complement integer).
- **Accumulator Type:** `int32_t` (32-bit signed accumulator).
- **Arithmetic & Overflow Semantics:** Standard C signed integer arithmetic. With input elements restricted to $[-30, 30]$ and $[-26, 26]$, individual element products satisfy $|a_i \cdot b_i| \le 780$. Over $N=256$ elements, the maximum possible accumulation magnitude cannot exceed $256 \times 780 = 199,680 \ll 2^{31}-1$, mathematically guaranteeing that no signed integer overflow can occur.
- **Problem Size:** $N = 256$ elements ($1024$ bytes per input vector).

## 4. Input Dataset & Expected Output
- **Input Data:** Deterministic pseudorandom test vectors generated via linear congruential sequences located in [`inputs/input_data.h`](inputs/input_data.h).
- **Expected Checksum / Result:**
  $$\text{Expected Dot Product} = -856$$
  Because this benchmark reduces to a single scalar result returned in `a0`, the element mismatch count (`element_mismatches`) is trivially 0 upon matching the expected checksum.

## 5. Host Compilation & Verification
To compile and execute the canonical C reference on the host machine:
```bash
gcc -O3 -Wall -Wextra benchmarks/dot_product/reference/dot_product.c -o /tmp/dot_product_host
/tmp/dot_product_host
```
The program will evaluate the kernel and verify the computed sum against both the precomputed constant (`-856`) and an independent loop calculation.

## 6. Bare-Metal RISC-V Compilation
The C reference implementation is compiled into standalone bare-metal RISC-V assembly using `riscv64-elf-gcc`:

```bash
# 1. Scalar RV32IM (-O3)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32im -mabi=ilp32 -O3 -S \
    benchmarks/dot_product/reference/dot_product.c \
    -o benchmarks/dot_product/compiler_output/dot_product_scalar_gcc.s

# 2. Vector RV32IMV (-O3, LMUL=m1)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32imv -mabi=ilp32 -O3 -mrvv-max-lmul=m1 -S \
    -fopt-info-vec-all=benchmarks/dot_product/compiler_output/vectorization_report.txt \
    benchmarks/dot_product/reference/dot_product.c \
    -o benchmarks/dot_product/compiler_output/dot_product_vector_gcc.s
```

## 7. KITES Simulation Assembly
Standalone, self-contained KITES assembly files are provided in [`kites_assembly/`](kites_assembly/):
- **Scalar Assembly:** [`dot_product_scalar_kites.s`](kites_assembly/dot_product_scalar_kites.s)
- **Vector Assembly:** [`dot_product_vector_kites.s`](kites_assembly/dot_product_vector_kites.s)

### Vector Kernel Adaptation Details:
1. **Accumulator Initialization:** Replaced unsupported `vmv.v.i v1, 0` with native `vxor.vv v1, v1, v1`.
2. **Reduction Seed:** Replaced unsupported `vmv.s.x v2, zero` with native `vxor.vv v2, v2, v2`.
3. **Scalar Result Extraction:** Replaced unsupported `vmv.x.s a0, v1` with vector memory spill `vse32.v v1, (sp)` followed by scalar integer load `lw a0, 0(sp)`.
4. **Syntax Normalization:** Stripped leading dots from branch labels and normalized unit-stride vector memory addressing syntax from `0(rs1)` to `(rs1)`.

### Execution Commands:
```bash
# Run Scalar Dot Product on KITES
build/cli/kites-cli benchmarks/dot_product/kites_assembly/dot_product_scalar_kites.s

# Run Vector Dot Product on KITES
build/cli/kites-cli benchmarks/dot_product/kites_assembly/dot_product_vector_kites.s
```

## 8. Simulation Results & Verification

Simulation metrics extracted from simulator execution steps and recorded in [`results/execution_summary.json`](results/execution_summary.json):

| Metric | Scalar (RV32IM) | Vector (RV32IMV) | Reduction / Speedup |
| :--- | :---: | :---: | :---: |
| **Kernel Dynamic Instructions** | 1,798 | 585 | **67.46% reduction ($3.07\times$ speedup)** |
| **Total Dynamic Instructions** | 1,804 | 591 | **67.24% reduction ($3.05\times$ speedup)** |
| **Harness Overhead Instructions** | 6 | 6 | Exact match |
| **Returned Result (`x10` / `a0`)** | `-856` (`0xFFFFFCA8`) | `-856` (`0xFFFFFCA8`) | **100% golden match** |
| **Element Mismatches** | 0 | 0 | 0 mismatches |
| **Simulator Errors** | 0 | 0 | Clean execution |


# KITES RISC-V Vector Extension Benchmark Suite

## 1. Overview & Purpose
This benchmark suite provides a rigorous, reproducible framework for evaluating and comparing scalar RISC-V (**RV32IM**) against vector-accelerated RISC-V (**RV32IMV**) execution on the KITES architectural simulator.

The primary objective is to evaluate:
- Dynamic instruction count reductions achieved by vectorization.
- Architectural vector execution behaviors under realistic, computationally meaningful workloads.
- Fidelity and compatibility of compiler-generated and hand-adapted RVV 1.0 assembly under KITES's implemented instruction subset and architectural constraints (`VLEN=128`, `LMUL=m1`).

---

## 2. Workload Suite Summary

| Workload | Domain | Kernel / Algorithm | Input Dimensions | Data Format | Memory Footprint |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`dot_product`** | Linear Algebra / DSP | $\sum a_i \cdot b_i$ (accumulated inner product) | $N = 256$ elements | `int32_t` | $2048$ bytes |
| **`matrix_multiplication`** | Numerical Methods / GEMM | $C = A \times B$ ($i$-$k$-$j$ contiguous row order) | $16 \times 16$ square matrices | `int32_t` | $3072$ bytes |
| **`fir_filter`** | Audio / Signal Processing | 1D discrete convolution with zero-padding | $256$ samples, $16$ taps | `int32_t` | $2112$ bytes |
| **`sobel`** | Computer Vision / Imaging | 2D spatial convolution ($3\times 3$ $G_x, G_y$), L1 norm | $32 \times 32$ grayscale image | `uint8_t` | $2048$ bytes |

### 2.1 Benchmark Performance & Speedup Summary

| Workload | Scalar Kernel | Vector Kernel | Kernel Speedup ($\times$) | Kernel Reduction ($\%$) | Total Speedup ($\times$) | Total Reduction ($\%$) | Verification Status | Simulator Errors |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Dot Product** ($N=256$) | 1,798 | 585 | **$3.07\times$** | **$67.46\%$** | $3.05\times$ | $67.24\%$ | **100% Match** (`a0 = -856`) | **0 errors** |
| **Matrix Multiplication** ($16 \times 16$) | 29,772 | 12,632 | **$2.36\times$** | **$57.57\%$** | $2.23\times$ | $55.17\%$ | **100% Match** (256/256 elems) | **0 errors** |
| **FIR Filter** (256s, 16t) | 30,134 | 10,391 | **$2.90\times$** | **$65.52\%$** | $2.69\times$ | $62.82\%$ | **100% Match** (256/256 elems) | **0 errors** |
| **Sobel Edge Detection** ($32 \times 32$) | 28,095 | 24,083 | **$1.17\times$** | **$14.28\%$** | $1.14\times$ | $12.07\%$ | **100% Match** (1024/1024 px) | **0 errors** |

*Detailed architectural evaluation, speedup breakdowns, and simulator insights are available in [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md).*

---

## 3. Directory Organization

```text
benchmarks/
├── README.md                          <- Suite overview, instructions & workflow guidelines
├── BENCHMARK_REPORT.md                <- Comprehensive benchmark evaluation & architectural report
├── visualizations/                    <- Publication-ready SVG/PNG plots & generator script
│   ├── generate_charts.py
│   ├── instruction_counts_comparison.{svg,png}
│   ├── speedup_and_reduction.{svg,png}
│   └── instruction_breakdown.{svg,png}
├── dot_product/
│   ├── README.md                      <- Workload specification, math definition & expected outputs
│   ├── reference/
│   │   └── dot_product.c              <- Canonical C reference implementation
│   ├── inputs/
│   │   └── input_data.h               <- Deterministic input vectors & golden reference values
│   ├── compiler_output/               <- GCC -O3 scalar and vector assembly & vectorization logs
│   ├── kites_assembly/                <- KITES-adapted standalone scalar & vector assembly
│   └── results/
│       └── execution_summary.json     <- Profiled instruction counts & speedup metrics
├── matrix_multiplication/
│   ├── README.md
│   ├── reference/
│   │   └── matrix_multiplication.c
│   ├── inputs/
│   │   └── input_data.h
│   ├── compiler_output/
│   ├── kites_assembly/
│   └── results/
│       └── execution_summary.json
├── fir_filter/
│   ├── README.md
│   ├── reference/
│   │   └── fir_filter.c
│   ├── inputs/
│   │   └── input_data.h
│   ├── compiler_output/
│   ├── kites_assembly/
│   └── results/
│       └── execution_summary.json
└── sobel/
    ├── README.md
    ├── reference/
    │   └── sobel.c
    ├── inputs/
    │   └── input_data.h
    ├── compiler_output/
    ├── kites_assembly/
    └── results/
        └── execution_summary.json
```

### Canonical Reference Integrity:
The files in each `reference/` directory (`dot_product.c`, `matrix_multiplication.c`, `fir_filter.c`, `sobel.c`) are the **canonical algorithmic references**. They must remain fixed and unmodified to guarantee that scalar and vector comparisons reflect identical computational work.

---

## 4. Benchmark Workflow & Execution Status

This project followed a rigorous, three-phase workflow. All phases are complete:

### Phase 1: Benchmark Organization & Canonical C References *(Completed)*
- Established the dedicated benchmark directory structure.
- Implemented bare-metal-ready, self-contained canonical C references for all four workloads without OS or dynamic memory dependencies.
- Defined deterministic input datasets and precomputed expected golden values.
- Validated all references via host compilation and execution.

### Phase 2: Assembly Generation, KITES Adaptation & Simulation *(Completed)*
- Compiled canonical C references into scalar RV32IM assembly using `riscv64-elf-gcc -march=rv32im -O3`.
- Compiled canonical C references into vector RV32IMV assembly using `riscv64-elf-gcc -march=rv32imv -O3 -mrvv-max-lmul=m1`.
- Preserved original compiler outputs and vectorization logs (`-fopt-info-vec`).
- Adapted generated assembly into KITES-compliant assembly by:
  - Stripping unsupported directives (`.file`, `.attribute`, `.size`, `.p2align`).
  - Correcting compiler label syntax (`.L3` $\rightarrow$ `L3`).
  - Normalizing unit-stride vector memory syntax (`vle32.v vd, 0(rs1)` $\rightarrow$ `vle32.v vd, (rs1)`).
  - Emulating unsupported vector moves (`vmv.*`) using supported primitives (`vxor.vv`, vector spills).
- Executed both scalar and vector versions in KITES via `kites-cli`.
- Validated that scalar and vector simulation results match the golden C outputs (100% element and checksum matches).
- Recorded dynamic instructions retired from the KITES hardware execution counters.

### Phase 3: Analysis & Visualization *(Completed)*
- Computed speedup and dynamic instruction count reduction percentages for both computational kernels and overall program runs.
- Generated publication-ready SVG and 300-DPI PNG visualization plots (`benchmarks/visualizations/`) comparing scalar vs. vector execution.
- Authored the comprehensive final benchmarking report in [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md).

---

## 5. Host Compilation & Verification Guide

Each benchmark includes a self-contained host test harness that runs the computational kernel, validates every output element, and checks an overall checksum.

You can validate all four benchmarks in one step:

```bash
# 1. Dot Product
gcc -O3 -Wall -Wextra benchmarks/dot_product/reference/dot_product.c -o /tmp/dot_product_test && /tmp/dot_product_test

# 2. Matrix Multiplication
gcc -O3 -Wall -Wextra benchmarks/matrix_multiplication/reference/matrix_multiplication.c -o /tmp/matmul_test && /tmp/matmul_test

# 3. FIR Digital Filter
gcc -O3 -Wall -Wextra benchmarks/fir_filter/reference/fir_filter.c -o /tmp/fir_test && /tmp/fir_test

# 4. Sobel Image Edge Detection
gcc -O3 -Wall -Wextra benchmarks/sobel/reference/sobel.c -o /tmp/sobel_test && /tmp/sobel_test
```

### Bare-Metal RISC-V Header Compatibility:
When compiling for bare-metal RISC-V (`-DBAREMETAL`), host headers (`<stdio.h>`, `<stdlib.h>`) and `main()` are excluded, exposing only the clean kernel function:
```bash
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32im -mabi=ilp32 -O3 -S benchmarks/dot_product/reference/dot_product.c -o /dev/null
```

---

## 6. Results Schema & Metrics Definition

Each benchmark produces an `execution_summary.json` file in its `results/` directory with a standardized schema across all four workloads:

- **Top-Level Metadata:**
  - `workload`: Descriptive name and parameters of the benchmark.
  - `data_size`: Number of primary elements or samples processed.
  - `expected_checksum`: Canonical integer reference checksum.
- **Execution Profiles (`scalar` and `vector`):**
  - `total_instructions`: Total dynamic instructions retired across the entire program execution (setup + kernel + checksum validation).
  - `kernel_instructions`: Dynamic instructions retired strictly inside the computational kernel function.
  - `harness_instructions`: Dynamic instructions retired outside the computational kernel (program setup, `jal`, and post-kernel checksum accumulation loop).
  - `checksum_result`: Final integer accumulator value returned in `a0` (`x10`).
  - `element_mismatches`: Number of output elements differing from the golden reference (trivially `0` for single-scalar dot product).
  - `errors`: Number of simulator execution errors reported by `errors_dump.json` (must be `0`).
  - `verified`: Boolean indicating bit-for-bit output match and zero simulator errors.
- **Comparative Metrics:**
  - `speedup_ratio`: Total instruction speedup, defined as $\text{total\_scalar} / \text{total\_vector}$. Includes harness overhead.
  - `kernel_speedup_ratio`: Computational kernel speedup, defined as $\text{kernel\_scalar} / \text{kernel\_vector}$. Measures vector speedup on the algorithm itself without harness dilution.
  - `total_reduction_percent`: Total dynamic instruction reduction: $\left(1 - \frac{\text{total\_vector}}{\text{total\_scalar}}\right) \times 100\%$.
  - `kernel_reduction_percent`: Kernel dynamic instruction reduction: $\left(1 - \frac{\text{kernel\_vector}}{\text{kernel\_scalar}}\right) \times 100\%$.


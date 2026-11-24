# Benchmark: Integer FIR Digital Filter (`fir_filter`)

## 1. Overview & Application Domain
The Finite Impulse Response (FIR) digital filter is an essential workload in audio processing, communications systems (channel equalization, pulse shaping), biomedical signal monitoring (ECG filtering), and instrumentation. It applies a linear-phase discrete-time convolution of an input discrete signal with a fixed finite-length impulse response.

## 2. Mathematical Definition
Given an input signal $x[n]$ and a causal filter with $K$ tap coefficients $h[k]$:
$$y[n] = \sum_{k=0}^{K-1} h[k] \cdot x[n - k]$$

### Boundary Conditions:
For $n - k < 0$, standard zero-padding (zero initial filter state) is applied:
$$x[m] = 0 \quad \text{for } m < 0$$
This is implemented by restricting the summation index $k$ to $\min(n + 1, K)$ during the initial transient ramp-up period ($n < K$).

## 3. Data Representation & Semantics
- **Element Type:** `int32_t` (32-bit signed two's complement integers for input samples, tap coefficients, and filtered output samples).
- **Accumulator Type:** `int32_t` (32-bit signed accumulator).
- **Arithmetic & Overflow Semantics:** Standard C signed integer arithmetic. The 16 tap coefficients have sum $\sum |h_k| = 266$. Input samples are bounded in $[-33, 33]$. Maximum instantaneous output magnitude is $|y[n]| \le 33 \times 266 = 8,778 \ll 2^{31}-1$, mathematically guaranteeing that no signed integer overflow can occur.
- **Problem Size:** $N = 256$ samples, $K = 16$ taps ($1024$ bytes input, $64$ bytes taps, $1024$ bytes output; $2112$ bytes total).

## 4. Input Dataset & Expected Output
- **Taps:** Symmetric 16-tap lowpass FIR filter coefficients defined in [`inputs/input_data.h`](inputs/input_data.h).
- **Inputs:** 256 deterministic pseudo-random signal samples in [`inputs/input_data.h`](inputs/input_data.h).
- **Expected Checksum:**
  $$\text{Checksum} = \sum_{n=0}^{255} y[n] = -8806$$
- Exact precomputed golden output array $y[n]$ is provided in [`inputs/input_data.h`](inputs/input_data.h).

## 5. Host Compilation & Verification
To compile and execute the canonical C reference on the host machine:
```bash
gcc -O3 -Wall -Wextra benchmarks/fir_filter/reference/fir_filter.c -o /tmp/fir_host
/tmp/fir_host
```
The program runs the FIR convolution kernel, compares every sample against the expected golden reference, and confirms the checksum.

## 6. Bare-Metal RISC-V Compilation
The C reference implementation is compiled into standalone bare-metal RISC-V assembly using `riscv64-elf-gcc`:

```bash
# 1. Scalar RV32IM (-O3)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32im -mabi=ilp32 -O3 -S \
    benchmarks/fir_filter/reference/fir_filter.c \
    -o benchmarks/fir_filter/compiler_output/fir_filter_scalar_gcc.s

# 2. Vector RV32IMV (-O3, LMUL=m1)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32imv -mabi=ilp32 -O3 -mrvv-max-lmul=m1 -S \
    -fopt-info-vec-all=benchmarks/fir_filter/compiler_output/vectorization_report.txt \
    benchmarks/fir_filter/reference/fir_filter.c \
    -o benchmarks/fir_filter/compiler_output/fir_filter_vector_gcc.s
```

## 7. Vector Kernel Implementation Details (Custom RVV Kernel)
Standalone, self-contained KITES assembly files are provided in [`kites_assembly/`](kites_assembly/):
- **Scalar Assembly:** [`fir_filter_scalar_kites.s`](kites_assembly/fir_filter_scalar_kites.s)
- **Vector Assembly:** [`fir_filter_vector_kites.s`](kites_assembly/fir_filter_vector_kites.s)

### Motivation for Custom Implementation:
GCC auto-vectorization across the inner tap dimension requires reverse indexing $x[n - k]$, causing the compiler to emit unsupported instructions and whole-register moves:
- `vrgather.vv` vector register gather instructions.
- `vid.v` index generation.
- `vl1re32.v` whole register loads.

Because these operations are outside KITES's implemented RVV subset, the compiler-generated vector assembly could not be executed directly. A custom hand-written RVV replacement kernel was developed that restructures the loop hierarchy from an inner-product reduction across taps to an outer-product tap accumulation along the sample dimension using `vmacc.vx`.

### Vector Kernel Design Highlights:
1. **Sample-Dimension Vectorization:** Instead of gathering taps across time, vectorization is structured along the sample dimension: for each tap $k \in [0, 15]$, a forward unit-stride vector slice is updated:
   $$\mathbf{y}[k \dots N-1] \mathrel{+}= \text{taps}[k] \times \mathbf{x}[0 \dots N-1-k]$$
2. **Unit-Stride Streaming & `vmacc.vx`:** Replaced all gathers and permutations with unit-stride vector memory loads (`vle32.v`), vector multiply-accumulate with a scalar tap operand (**`vmacc.vx`**), and unit-stride vector stores (`vse32.v`). Vector length is dynamically negotiated via `vsetvli`.
3. **Output Buffer Zeroing:** Output buffer is initialized in 192 dynamic instructions using `vxor.vv v1, v1, v1` and `vse32.v` with `LMUL=m1`.
4. **Syntax Normalization:** Conforms to KITES assembly syntax (undotted labels, unit-stride addressing `(rs1)`, no unsupported directives).

### Execution Commands:
```bash
# Run Scalar FIR Filter on KITES
build/cli/kites-cli benchmarks/fir_filter/kites_assembly/fir_filter_scalar_kites.s

# Run Vector FIR Filter on KITES (with full memory dump of output samples)
build/cli/kites-cli benchmarks/fir_filter/kites_assembly/fir_filter_vector_kites.s --mem-dump 0x10000440 128
```

## 8. Simulation Results & Verification

Simulation metrics extracted from simulator execution steps and recorded in [`results/execution_summary.json`](results/execution_summary.json):

| Metric | Scalar (RV32IM) | Vector (RV32IMV) | Reduction / Speedup |
| :--- | :---: | :---: | :---: |
| **Kernel Dynamic Instructions** | 30,134 | 10,391 | **65.52% reduction ($2.90\times$ speedup)** |
| **Total Dynamic Instructions** | 31,429 | 11,686 | **62.82% reduction ($2.69\times$ speedup)** |
| **Harness Overhead Instructions** | 1,295 | 1,295 | Exact match |
| **Returned Checksum (`x10` / `a0`)** | `-8806` (`0xFFFFDD9A`) | `-8806` (`0xFFFFDD9A`) | **100% golden match** |
| **Sample Mismatches** | 0 / 256 | 0 / 256 | **100% bit-for-bit match** |
| **Simulator Errors** | 0 | 0 | Clean execution |


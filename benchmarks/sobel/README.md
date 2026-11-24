# Benchmark: Sobel Image Edge Detection (`sobel`)

## 1. Overview & Application Domain
The Sobel operator is a classical 2D spatial convolution filter used extensively in image processing, computer vision, robotics (feature extraction, obstacle detection), and medical imaging. It approximates the intensity gradient magnitude at each pixel in a grayscale image to identify object boundaries and structural contours.

## 2. Mathematical Definition & Kernels
The algorithm convolves the input image with two orthogonal $3 \times 3$ integer kernels to compute horizontal ($G_x$) and vertical ($G_y$) spatial derivatives:

$$G_x = \begin{bmatrix} -1 & 0 & +1 \\ -2 & 0 & +2 \\ -1 & 0 & +1 \end{bmatrix}, \quad G_y = \begin{bmatrix} +1 & +2 & +1 \\ 0 & 0 & 0 \\ -1 & -2 & -1 \end{bmatrix}$$

For a pixel $(x, y)$ in the interior ($1 \le x < W - 1$, $1 \le y < H - 1$):
$$G_x = (P_{x+1, y-1} - P_{x-1, y-1}) + 2 \cdot (P_{x+1, y} - P_{x-1, y}) + (P_{x+1, y+1} - P_{x-1, y+1})$$
$$G_y = (P_{x-1, y-1} - P_{x-1, y+1}) + 2 \cdot (P_{x, y-1} - P_{x, y+1}) + (P_{x+1, y-1} - P_{x+1, y+1})$$

### Gradient Magnitude Approximation:
To eliminate floating-point arithmetic and costly square-root computations on embedded integer hardware, the gradient magnitude is approximated using the **Manhattan / L1 norm**:
$$G = |G_x| + |G_y|$$

### Output Representation & Clamping:
$G_x$ and $G_y$ range within $[-1020, 1020]$, meaning $G \in [0, 2040]$. The magnitude is clamped to the 8-bit unsigned range $[0, 255]$:
$$\text{output}[y][x] = \begin{cases} 255 & \text{if } G > 255 \\ G & \text{otherwise} \end{cases}$$

### Border Handling:
Outer boundary pixels ($x = 0$, $x = W - 1$, $y = 0$, $y = H - 1$) lack a full $3 \times 3$ neighborhood. The benchmark uses a strict **zero-border policy** where all perimeter pixels are assigned `0` in the output image.

## 3. Data Representation & Semantics
- **Pixel Type:** `uint8_t` (8-bit grayscale intensity values in $[0, 255]$).
- **Intermediate Gradients:** `int32_t` (standard C signed 32-bit arithmetic). Because pixel values are bounded in $[0, 255]$, intermediate gradient sums strictly remain within $[-1020, 1020]$ and magnitude within $[0, 2040]$, mathematically guaranteeing that no signed integer overflow can occur.
- **Problem Size:** $32 \times 32$ pixels ($1024$ bytes input image, $1024$ bytes output image; $2048$ bytes total).

## 4. Input Dataset & Expected Output
- **Input Image:** A deterministic $32 \times 32$ synthetic grayscale test pattern defined in [`inputs/input_data.h`](inputs/input_data.h) featuring an intensity baseline ($30$), an inner square ($200$), and intersecting diagonal lines ($120$) to trigger vertical, horizontal, and oblique edges.
- **Expected Checksum:**
  $$\text{Checksum} = \sum_{i=0}^{1023} \text{output}[i] = 53140$$
- Complete precomputed golden output image is provided in [`inputs/input_data.h`](inputs/input_data.h).

## 5. Host Compilation & Verification
To compile and execute the canonical C reference on the host machine:
```bash
gcc -O3 -Wall -Wextra benchmarks/sobel/reference/sobel.c -o /tmp/sobel_host
/tmp/sobel_host
```
The program executes the filter, verifies every pixel against the expected golden reference, and confirms the overall edge pixel checksum.

## 6. Bare-Metal RISC-V Compilation
The C reference implementation is compiled into standalone bare-metal RISC-V assembly using `riscv64-elf-gcc`:

```bash
# 1. Scalar RV32IM (-O3)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32im -mabi=ilp32 -O3 -S \
    benchmarks/sobel/reference/sobel.c \
    -o benchmarks/sobel/compiler_output/sobel_scalar_gcc.s

# 2. Vector RV32IMV (-O3, LMUL=m1)
riscv64-elf-gcc -ffreestanding -DBAREMETAL -march=rv32imv -mabi=ilp32 -O3 -mrvv-max-lmul=m1 -S \
    -fopt-info-vec-all=benchmarks/sobel/compiler_output/vectorization_report.txt \
    benchmarks/sobel/reference/sobel.c \
    -o benchmarks/sobel/compiler_output/sobel_vector_gcc.s
```

## 7. Vector Kernel Implementation Details (Custom RVV Kernel)
Standalone, self-contained KITES assembly files are provided in [`kites_assembly/`](kites_assembly/):
- **Scalar Assembly:** [`sobel_scalar_kites.s`](kites_assembly/sobel_scalar_kites.s)
- **Vector Assembly:** [`sobel_vector_kites.s`](kites_assembly/sobel_vector_kites.s)

### Motivation for Custom Implementation:
GCC auto-vectorization generated vector code relying on instructions and fractional register groupings not supported by KITES:
- Widening integer additions/subtractions (`vwaddu.vv`, `vwsubu.vv`).
- Narrowing integer logical right shifts (`vnsrl.wi`).
- Fractional LMUL configurations (`mf2`).

Because these instructions are outside KITES's implemented RVV subset, the compiler-generated vector assembly could not be executed directly. A custom hand-written RVV replacement kernel was developed, strictly conforming to single-width operations at `LMUL=m1`.

### Vector Kernel Design Highlights:
1. **Handling Lack of Widening/Narrowing Operations:** To execute within KITES constraints (`SEW=32`, `LMUL=m1`), interior pixels are processed in 4-pixel chunks using 32-bit vector registers with software byte-to-word unpack and pack routines.
2. **Zero-Border Initialization:** Output image buffer is initialized in only 192 instructions using `vse8.v` with `LMUL=m1`, satisfying the zero-border policy before stencil calculation.
3. **In-Register Vector Gradient Arithmetic:**
   - Gradients $G_x$ and $G_y$ are computed across 4 pixels simultaneously using `vsub.vv`, `vsll.vi`, and `vadd.vv`.
   - Absolute values $|G_x|, |G_y|$ are computed in-register using `vrsub.vi` and `vmax.vv`.
   - $L_1$ norm magnitude ($|G_x| + |G_y|$) is computed using `vadd.vv`.
   - Saturation clamping to 255 is performed in vector registers via unsigned minimum `vminu.vx`.
4. **Syntax Normalization:** Conforms to KITES assembly syntax (undotted labels, unit-stride addressing `(rs1)`, no unsupported directives).

### Execution Commands:
```bash
# Run Scalar Sobel Filter on KITES
build/cli/kites-cli benchmarks/sobel/kites_assembly/sobel_scalar_kites.s

# Run Vector Sobel Filter on KITES (with full memory dump of output pixels)
build/cli/kites-cli benchmarks/sobel/kites_assembly/sobel_vector_kites.s --mem-dump 0x10000400 128
```

## 8. Simulation Results & Verification

Simulation metrics extracted from simulator execution steps and recorded in [`results/execution_summary.json`](results/execution_summary.json):

| Metric | Scalar (RV32IM) | Vector (RV32IMV) | Reduction / Speedup |
| :--- | :---: | :---: | :---: |
| **Kernel Dynamic Instructions** | 28,095 | 24,083 | **14.28% reduction ($1.17\times$ speedup)** |
| **Total Dynamic Instructions** | 33,228 | 29,216 | **12.07% reduction ($1.14\times$ speedup)** |
| **Harness Overhead Instructions** | 5,133 | 5,133 | Exact match |
| **Returned Checksum (`x10` / `a0`)** | `53140` (`0x0000CF94`) | `53140` (`0x0000CF94`) | **100% golden match** |
| **Pixel Mismatches** | 0 / 1024 | 0 / 1024 | **100% bit-for-bit match** |
| **Simulator Errors** | 0 | 0 | Clean execution |


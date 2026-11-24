/**
 * @file matrix_multiplication.c
 * @brief Canonical C reference implementation of integer matrix multiplication for KITES.
 *
 * Algorithm:
 *   Square matrix multiplication C = A * B where A, B, C are N x N matrices
 *   stored in contiguous row-major format:
 *     C[i][j] = \sum_{k=0}^{N-1} A[i][k] * B[k][j]
 *
 *   The loop order i-k-j enables contiguous unit-stride vector memory access
 *   across rows of B and C, pairing with scalar broadcast / multiply-accumulate
 *   of elements from A (vmacc.vx / vmacc.vv).
 *
 * Arithmetic Semantics:
 *   - Element type: int32_t (32-bit signed integer).
 *   - Accumulator type: int32_t (32-bit signed integer accumulation).
 *   - Overflow: Standard C signed integer arithmetic. The bounded test inputs
 *     guarantee that each inner product sum strictly remains within
 *     [-2^31, 2^31 - 1], so no signed integer overflow can occur.
 *   - Problem size: N = 16 (16 x 16 matrices = 256 elements each).
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Multiplies two N x N integer matrices: C = A * B.
 *
 * @param a Pointer to input matrix A (row-major, N x N).
 * @param b Pointer to input matrix B (row-major, N x N).
 * @param c Pointer to output matrix C (row-major, N x N).
 * @param n Dimension of the square matrices.
 */
void matrix_multiply(const int32_t * restrict a,
                     const int32_t * restrict b,
                     int32_t * restrict c,
                     uint32_t n) {
    const uint32_t total_elements = n * n;
    for (uint32_t idx = 0; idx < total_elements; ++idx) {
        c[idx] = 0;
    }

    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t k = 0; k < n; ++k) {
            const int32_t aik = a[i * n + k];
            for (uint32_t j = 0; j < n; ++j) {
                c[i * n + j] += aik * b[k * n + j];
            }
        }
    }
}

#ifndef BAREMETAL
#include <stdio.h>
#include <stdlib.h>
#include "../inputs/input_data.h"

int main(void) {
    printf("[Matrix Multiplication] Running reference implementation (%ux%u)...\n",
           MATMUL_DIM, MATMUL_DIM);

    int32_t computed_c[MATMUL_ELEMENTS];
    matrix_multiply(matmul_a, matmul_b, computed_c, MATMUL_DIM);

    // Verify all 256 elements against independent expected output
    int mismatches = 0;
    int32_t actual_checksum = 0;
    for (uint32_t idx = 0; idx < MATMUL_ELEMENTS; ++idx) {
        actual_checksum += computed_c[idx];
        if (computed_c[idx] != matmul_expected_c[idx]) {
            if (mismatches < 5) {
                fprintf(stderr, "  Mismatch at index %u: expected %d, got %d\n",
                        idx, matmul_expected_c[idx], computed_c[idx]);
            }
            mismatches++;
        }
    }

    printf("  Expected Checksum: %d\n", MATMUL_EXPECTED_CHECKSUM);
    printf("  Actual Checksum:   %d\n", actual_checksum);

    if (mismatches > 0 || actual_checksum != MATMUL_EXPECTED_CHECKSUM) {
        fprintf(stderr, "ERROR: Matrix multiplication failed (%d mismatches)!\n", mismatches);
        return EXIT_FAILURE;
    }

    printf("SUCCESS: All 256 matrix elements and checksum verified!\n");
    return EXIT_SUCCESS;
}
#endif

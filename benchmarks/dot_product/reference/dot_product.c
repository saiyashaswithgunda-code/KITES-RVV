/**
 * @file dot_product.c
 * @brief Canonical C reference implementation of integer dot product for KITES.
 *
 * Algorithm:
 *   dot_product = \sum_{i=0}^{N-1} a[i] * b[i]
 *
 * Arithmetic Semantics:
 *   - Element type: int32_t (32-bit signed two's complement integer).
 *   - Accumulator type: int32_t (32-bit signed integer accumulation).
 *   - Overflow: Standard C signed integer arithmetic. The bounded test inputs
 *     guarantee that intermediate products and the accumulated sum strictly
 *     remain within [-2^31, 2^31 - 1], so no signed integer overflow can occur.
 *   - Problem size: N = 256 elements.
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Computes the integer dot product of two vectors.
 *
 * @param a Pointer to first array of int32_t.
 * @param b Pointer to second array of int32_t.
 * @param n Number of elements.
 * @return int32_t Accumulated dot product sum.
 */
int32_t dot_product(const int32_t *a, const int32_t *b, uint32_t n) {
    int32_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

#ifndef BAREMETAL
#include <stdio.h>
#include <stdlib.h>
#include "../inputs/input_data.h"

int main(void) {
    printf("[Dot Product] Running reference implementation (N = %u)...\n", DOT_PRODUCT_N);

    int32_t actual = dot_product(dot_product_a, dot_product_b, DOT_PRODUCT_N);

    // Independent golden check
    int32_t reference = 0;
    for (uint32_t i = 0; i < DOT_PRODUCT_N; ++i) {
        reference += dot_product_a[i] * dot_product_b[i];
    }

    printf("  Expected: %d\n", DOT_PRODUCT_EXPECTED);
    printf("  Actual:   %d\n", actual);

    if (actual != DOT_PRODUCT_EXPECTED || actual != reference) {
        fprintf(stderr, "ERROR: Dot product mismatch (expected %d, got %d)!\n",
                DOT_PRODUCT_EXPECTED, actual);
        return EXIT_FAILURE;
    }

    printf("SUCCESS: Dot product verified against independent reference!\n");
    return EXIT_SUCCESS;
}
#endif

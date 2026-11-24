/**
 * @file fir_filter.c
 * @brief Canonical C reference implementation of a 1D Finite Impulse Response (FIR) filter for KITES.
 *
 * Algorithm:
 *   Discrete 1D convolution:
 *     y[n] = \sum_{k=0}^{K-1} h[k] * x[n - k]
 *   where h[k] are the K filter tap coefficients and x[m] is the input sequence.
 *   For n - k < 0 (boundary pre-history), zero-padding is applied: x[m] = 0 for m < 0.
 *
 * Arithmetic Semantics:
 *   - Element type: int32_t (32-bit signed integers for inputs, taps, and outputs).
 *   - Accumulator type: int32_t (32-bit signed integer accumulation).
 *   - Overflow: Standard C signed integer arithmetic. The bounded test inputs
 *     and coefficients guarantee that output samples strictly remain within
 *     [-2^31, 2^31 - 1], so no signed integer overflow can occur.
 *   - Problem size: N = 256 input samples, K = 16 taps.
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Computes 1D FIR digital filtering over an array of samples.
 *
 * @param input Pointer to input signal samples (length num_samples).
 * @param taps Pointer to filter tap coefficients (length num_taps).
 * @param output Pointer to destination output array (length num_samples).
 * @param num_samples Total number of signal samples (256).
 * @param num_taps Number of filter tap coefficients (16).
 */
void fir_filter(const int32_t * restrict input,
                const int32_t * restrict taps,
                int32_t * restrict output,
                uint32_t num_samples,
                uint32_t num_taps) {
    for (uint32_t n = 0; n < num_samples; ++n) {
        int32_t acc = 0;
        const uint32_t max_k = (n < num_taps) ? (n + 1) : num_taps;
        for (uint32_t k = 0; k < max_k; ++k) {
            acc += taps[k] * input[n - k];
        }
        output[n] = acc;
    }
}

#ifndef BAREMETAL
#include <stdio.h>
#include <stdlib.h>
#include "../inputs/input_data.h"

int main(void) {
    printf("[FIR Filter] Running reference implementation (%u samples, %u taps)...\n",
           FIR_NUM_SAMPLES, FIR_NUM_TAPS);

    int32_t computed_outputs[FIR_NUM_SAMPLES];
    fir_filter(fir_inputs, fir_taps, computed_outputs, FIR_NUM_SAMPLES, FIR_NUM_TAPS);

    // Verify all 256 output samples
    int mismatches = 0;
    int32_t actual_checksum = 0;
    for (uint32_t idx = 0; idx < FIR_NUM_SAMPLES; ++idx) {
        actual_checksum += computed_outputs[idx];
        if (computed_outputs[idx] != fir_expected_outputs[idx]) {
            if (mismatches < 5) {
                fprintf(stderr, "  Mismatch at sample %u: expected %d, got %d\n",
                        idx, fir_expected_outputs[idx], computed_outputs[idx]);
            }
            mismatches++;
        }
    }

    printf("  Expected Checksum: %d\n", FIR_EXPECTED_CHECKSUM);
    printf("  Actual Checksum:   %d\n", actual_checksum);

    if (mismatches > 0 || actual_checksum != FIR_EXPECTED_CHECKSUM) {
        fprintf(stderr, "ERROR: FIR filter failed (%d mismatches)!\n", mismatches);
        return EXIT_FAILURE;
    }

    printf("SUCCESS: All 256 filtered samples and checksum verified!\n");
    return EXIT_SUCCESS;
}
#endif

/**
 * @file sobel.c
 * @brief Canonical C reference implementation of Sobel 2D edge detection for KITES.
 *
 * Algorithm:
 *   Applies 3x3 horizontal (Gx) and vertical (Gy) Sobel spatial convolution kernels
 *   to compute the image intensity gradient approximation:
 *
 *       Gx = [ -1  0 +1 ]       Gy = [ +1 +2 +1 ]
 *            [ -2  0 +2 ]            [  0  0  0 ]
 *            [ -1  0 +1 ]            [ -1 -2 -1 ]
 *
 * Gradient Magnitude Approximation:
 *   Manhattan / L1 norm distance:
 *     G = |Gx| + |Gy|
 *   Avoids floating-point operations and square root computations.
 *   The magnitude G is clamped to [0, 255] and stored as an 8-bit unsigned integer:
 *     output[y][x] = (G > 255) ? 255 : (uint8_t)G
 *
 * Border Handling:
 *   Zero-border policy: Outer perimeter pixels (x = 0, x = width - 1, y = 0, y = height - 1)
 *   are assigned 0 in the output image.
 *
 * Arithmetic Semantics:
 *   - Input / Output pixels: uint8_t (8-bit grayscale intensity [0, 255]).
 *   - Intermediate Gradients: int32_t (standard C signed 32-bit arithmetic).
 *     Because pixel values are bounded in [0, 255], intermediate gradient sums
 *     strictly remain within [-1020, 1020] and magnitude within [0, 2040],
 *     guaranteeing no signed integer overflow can occur.
 *   - Problem size: 32 x 32 grayscale image (1024 bytes).
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Computes 2D Sobel edge detection over a grayscale image.
 *
 * @param input Pointer to input image (width x height bytes).
 * @param output Pointer to output edge magnitude image (width x height bytes).
 * @param width Image width in pixels (32).
 * @param height Image height in pixels (32).
 */
void sobel_filter(const uint8_t * restrict input,
                  uint8_t * restrict output,
                  uint32_t width,
                  uint32_t height) {
    const uint32_t total_pixels = width * height;
    for (uint32_t i = 0; i < total_pixels; ++i) {
        output[i] = 0;
    }

    for (uint32_t y = 1; y < height - 1; ++y) {
        const uint32_t row_prev = (y - 1) * width;
        const uint32_t row_curr = y * width;
        const uint32_t row_next = (y + 1) * width;

        for (uint32_t x = 1; x < width - 1; ++x) {
            const int32_t p00 = input[row_prev + (x - 1)];
            const int32_t p01 = input[row_prev + x];
            const int32_t p02 = input[row_prev + (x + 1)];

            const int32_t p10 = input[row_curr + (x - 1)];
            const int32_t p12 = input[row_curr + (x + 1)];

            const int32_t p20 = input[row_next + (x - 1)];
            const int32_t p21 = input[row_next + x];
            const int32_t p22 = input[row_next + (x + 1)];

            const int32_t gx = (p02 - p00) + 2 * (p12 - p10) + (p22 - p20);
            const int32_t gy = (p00 - p20) + 2 * (p01 - p21) + (p02 - p22);

            const int32_t abs_gx = (gx < 0) ? -gx : gx;
            const int32_t abs_gy = (gy < 0) ? -gy : gy;
            int32_t mag = abs_gx + abs_gy;

            if (mag > 255) {
                mag = 255;
            }

            output[row_curr + x] = (uint8_t)mag;
        }
    }
}

#ifndef BAREMETAL
#include <stdio.h>
#include <stdlib.h>
#include "../inputs/input_data.h"

int main(void) {
    printf("[Sobel Filter] Running reference implementation (%ux%u pixels)...\n",
           SOBEL_WIDTH, SOBEL_HEIGHT);

    uint8_t computed_output[SOBEL_PIXELS];
    sobel_filter(sobel_input_image, computed_output, SOBEL_WIDTH, SOBEL_HEIGHT);

    // Verify all 1024 output pixels
    int mismatches = 0;
    uint32_t actual_checksum = 0;
    for (uint32_t idx = 0; idx < SOBEL_PIXELS; ++idx) {
        actual_checksum += computed_output[idx];
        if (computed_output[idx] != sobel_expected_output[idx]) {
            if (mismatches < 5) {
                fprintf(stderr, "  Mismatch at pixel %u (x=%u, y=%u): expected %u, got %u\n",
                        idx, idx % SOBEL_WIDTH, idx / SOBEL_WIDTH,
                        sobel_expected_output[idx], computed_output[idx]);
            }
            mismatches++;
        }
    }

    printf("  Expected Checksum: %u\n", SOBEL_EXPECTED_CHECKSUM);
    printf("  Actual Checksum:   %u\n", actual_checksum);

    if (mismatches > 0 || actual_checksum != SOBEL_EXPECTED_CHECKSUM) {
        fprintf(stderr, "ERROR: Sobel filter failed (%d mismatches)!\n", mismatches);
        return EXIT_FAILURE;
    }

    printf("SUCCESS: All 1024 pixels and edge checksum verified!\n");
    return EXIT_SUCCESS;
}
#endif

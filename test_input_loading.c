/*
 * Test input loading and pre-whitening
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Test wrapper that captures state after byteslice
extern void test_capture_after_byteslice(uint8_t *ab_out, uint8_t *cd_out,
                                         const uint8_t *input, uint64_t prewhiten_key);

void print_vector_state(const char *label, const uint8_t *data, int num_vectors) {
    printf("%s:\n", label);
    for (int i = 0; i < num_vectors; i++) {
        printf("  v%2d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", data[i * 16 + j]);
        }
        printf("\n");
    }
}

int main(void) {
    uint8_t input[256];
    uint8_t ab_out[128];  // 8 vectors
    uint8_t cd_out[128];  // 8 vectors
    uint64_t prewhiten_key = 0xd20d72f2af5286b2ULL;

    printf("========================================\n");
    printf("Input Loading and Byteslice Test\n");
    printf("========================================\n\n");

    // Prepare input
    printf("Step 1: Prepare input (distinctive pattern)\n");
    for (int i = 0; i < 256; i++) {
        input[i] = i;
    }
    printf("  Input block 0: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", input[i]);
    }
    printf("\n");
    printf("  Input block 1: ");
    for (int i = 16; i < 32; i++) {
        printf("%02x", input[i]);
    }
    printf("\n");
    printf("  Pre-whiten key: %016lx\n\n", prewhiten_key);

    // Call test function
    printf("Step 2: Load, pre-whiten, and byteslice\n");
    test_capture_after_byteslice(ab_out, cd_out, input, prewhiten_key);

    printf("\nStep 3: Check AB state (first 4 vectors):\n");
    print_vector_state("AB", ab_out, 4);

    printf("\nStep 4: Check CD state (first 4 vectors):\n");
    print_vector_state("CD", cd_out, 4);

    return 0;
}

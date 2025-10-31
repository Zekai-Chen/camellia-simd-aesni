/*
 * Test byteslice without prewhiten through test_initial_state_asm path
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Test with modified assembly that skips prewhiten
extern void test_byteslice_only_v2(uint8_t *ab_out, uint8_t *cd_out, const uint8_t *input);

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    uint8_t input[256];
    uint8_t ab[128], cd[128];

    printf("Testing byteslice through test_initial_state path (no prewhiten)\n\n");

    printf("Buffer addresses:\n");
    printf("  input: %p\n", (void*)input);
    printf("  ab:    %p\n", (void*)ab);
    printf("  cd:    %p\n", (void*)cd);
    printf("  diff:  %ld bytes\n\n", (char*)cd - (char*)ab);

    // Prepare input
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    // Initialize ab and cd with recognizable pattern
    memset(ab, 0xAA, 128);
    memset(cd, 0xCC, 128);

    test_byteslice_only_v2(ab, cd, input);

    printf("AB vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, ab);
    }
    printf("\nCD vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, cd);
    }

    printf("\nExpected AB: 01 45 23 67 fe ba dc 98\n");
    printf("Expected CD: 89 cd ab ef 76 32 54 10\n");

    return 0;
}

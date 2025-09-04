#include <stdio.h>
#include <time.h>
#include <stdint.h>

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// More realistic function that does some work to prevent optimization
__attribute__((noinline))
int dummy_function_with_work(void *a, void *b, void *c, uint64_t d) {
    // Do some minimal work to prevent compiler optimization
    volatile int x = (uintptr_t)a + (uintptr_t)b + (uintptr_t)c + d;
    return x & 1;
}

// Completely empty function - likely to be optimized away
__attribute__((noinline))
int dummy_function_empty(void *a, void *b, void *c, uint64_t d) {
    return 0;
}

// Function with register pressure similar to our Assembly function
__attribute__((noinline))
int dummy_function_heavy(void *ctx, void *dst, void *src, uint64_t count) {
    // Simulate register saving/restoring and some computation
    volatile uint64_t r1 = (uintptr_t)ctx;
    volatile uint64_t r2 = (uintptr_t)dst;
    volatile uint64_t r3 = (uintptr_t)src;
    volatile uint64_t r4 = count;
    volatile uint64_t r5 = r1 ^ r2;
    volatile uint64_t r6 = r3 ^ r4;
    volatile uint64_t r7 = r5 + r6;
    volatile uint64_t r8 = r7 * 13;
    return (r8 & 0xFF);
}

int main() {
    printf("🔍 Detailed Function Call Overhead Analysis\n");
    printf("===========================================\n");
    
    int iterations = 1000000;
    double start, end, total_time;
    volatile int result = 0; // Prevent optimization
    
    // Test 1: Empty function (likely optimized away)
    printf("\n1. Testing empty function (may be optimized away):\n");
    start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        result += dummy_function_empty((void*)1, (void*)2, (void*)3, 4);
    }
    end = get_time_seconds();
    total_time = end - start;
    printf("   Total time: %.6f seconds\n", total_time);
    printf("   Per call: %.3f nanoseconds\n", (total_time * 1e9) / iterations);
    
    // Test 2: Function with minimal work
    printf("\n2. Testing function with minimal work:\n");
    start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        result += dummy_function_with_work((void*)(uintptr_t)i, (void*)2, (void*)3, 4);
    }
    end = get_time_seconds();
    total_time = end - start;
    printf("   Total time: %.6f seconds\n", total_time);
    printf("   Per call: %.3f nanoseconds\n", (total_time * 1e9) / iterations);
    
    // Test 3: Function with heavier register usage
    printf("\n3. Testing function with heavy register usage:\n");
    start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        result += dummy_function_heavy((void*)(uintptr_t)i, (void*)2, (void*)3, 4);
    }
    end = get_time_seconds();
    total_time = end - start;
    double ns_per_call_heavy = (total_time * 1e9) / iterations;
    printf("   Total time: %.6f seconds\n", total_time);
    printf("   Per call: %.3f nanoseconds\n", ns_per_call_heavy);
    
    // Test 4: Baseline - just the loop overhead
    printf("\n4. Testing baseline loop overhead:\n");
    start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        result += i & 1; // Minimal work to prevent loop optimization
    }
    end = get_time_seconds();
    total_time = end - start;
    printf("   Total time: %.6f seconds\n", total_time);
    printf("   Per loop iteration: %.3f nanoseconds\n", (total_time * 1e9) / iterations);
    
    printf("\n📊 Analysis:\n");
    printf("============\n");
    printf("For our camellia function (10,000 calls):\n");
    printf("- Function call overhead: %.1f microseconds\n", (ns_per_call_heavy * 10000) / 1000);
    printf("- Total encryption time: ~10,500 microseconds\n");
    printf("- Call overhead percentage: %.3f%%\n", (ns_per_call_heavy * 10000) / 10500);
    
    printf("\nExpected realistic function call overhead: 5-20 nanoseconds\n");
    printf("If results show <1ns, the test is likely flawed due to compiler optimization.\n");
    
    printf("\nResult sink (prevent optimization): %d\n", result);
    
    return 0;
}
#!/usr/bin/env bash
# Comprehensive Camellia AArch64 SIMD Benchmark Script
#
# Runs full test matrix:
# - All key sizes: 128, 192, 256
# - All operations: enc, dec
# - All batch sizes: 1, 4, 8, 16
# - Reference vs Assembly

set -euo pipefail

# Configuration
EXE="${1:-./bench_camellia_a64}"
PIN="${PIN_CPU:-}"  # Set PIN_CPU=7 to use taskset -c 7
BYTES_LARGE="2Gi"
BYTES_SMALL="512Mi"

# Check if binary exists
if [[ ! -x "$EXE" ]]; then
    echo "Error: Benchmark binary not found: $EXE"
    echo "Run: make -f bench.mk bench"
    exit 1
fi

# Optional CPU pinning
if [[ -n "$PIN" ]]; then
    echo "Info: Pinning to CPU core $PIN"
    PIN_CMD="taskset -c $PIN"
else
    PIN_CMD=""
fi

echo "========================================="
echo "  Camellia AArch64 Benchmark Suite"
echo "========================================="
echo ""
echo "Binary: $EXE"
echo "Large dataset: $BYTES_LARGE"
echo "Small dataset: $BYTES_SMALL"
echo ""

# Baseline: C reference implementation (short run)
echo "========================================="
echo "  BASELINE: C Reference Implementation"
echo "========================================="
echo ""

for ks in 128 192 256; do
    echo "-- REF enc ks=$ks --"
    $PIN_CMD $EXE --impl ref --op enc --ks $ks --bytes $BYTES_SMALL --batch 16
    echo ""
done

# Main test: Assembly implementation - all configurations
echo "========================================="
echo "  MAIN: AArch64 Assembly (Crypto Ext)"
echo "========================================="
echo ""

# Test 1: All key sizes, encryption, batch=16 (optimal)
echo "--- All key sizes (enc, batch=16) ---"
for ks in 128 192 256; do
    echo "-- ASM enc ks=$ks batch=16 --"
    $PIN_CMD $EXE --impl asm --op enc --ks $ks --bytes $BYTES_LARGE --batch 16
    echo ""
done

# Test 2: Decryption with all key sizes, batch=16
echo "--- All key sizes (dec, batch=16) ---"
for ks in 128 192 256; do
    echo "-- ASM dec ks=$ks batch=16 --"
    $PIN_CMD $EXE --impl asm --op dec --ks $ks --bytes $BYTES_LARGE --batch 16
    echo ""
done

# Test 3: Batch size scaling (128-bit key, encryption)
echo "--- Batch size scaling (ks=128, enc) ---"
for batch in 1 4 8 16; do
    echo "-- ASM enc ks=128 batch=$batch --"
    $PIN_CMD $EXE --impl asm --op enc --ks 128 --bytes $BYTES_LARGE --batch $batch
    echo ""
done

echo "========================================="
echo "  Benchmark Complete"
echo "========================================="
echo ""
echo "Summary:"
echo "  - C reference baseline for 128/192/256-bit keys"
echo "  - Assembly enc/dec for all key sizes (optimal batch=16)"
echo "  - Batch scaling analysis (1/4/8/16)"
echo ""
echo "To analyze with perf:"
echo "  make -f bench.mk perf-bench"
echo ""
echo "To pin to specific CPU (e.g., big core on big.LITTLE):"
echo "  PIN_CPU=7 $0"
echo ""

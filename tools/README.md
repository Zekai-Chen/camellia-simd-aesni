# Camellia AArch64 Benchmark Tools

This directory contains performance benchmarking tools for the Camellia SIMD implementation.

## Quick Start

### Build

```bash
cd ..
make -f bench.mk bench
```

### Run Quick Benchmark

```bash
# Quick test (small dataset)
./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 256Mi --batch 16

# Full benchmark (2 GiB, optimal for throughput measurement)
./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
```

### Run Full Benchmark Suite

```bash
# All key sizes, operations, and batch sizes
./tools/bench_all_a64.sh

# Or via Makefile
make -f bench.mk full-bench
```

### Run with Performance Counters

```bash
make -f bench.mk perf-bench
```

## Files

- **bench_camellia_a64.c** - Main benchmark program
  - Supports C reference vs. AArch64 assembly comparison
  - Configurable key sizes (128/192/256-bit)
  - Configurable batch sizes (1/4/8/16)
  - Warm-up and timing phases
  - Aligned memory allocation for optimal performance

- **bench_all_a64.sh** - Comprehensive benchmark script
  - Runs full test matrix
  - C reference baseline
  - Assembly enc/dec for all key sizes
  - Batch size scaling analysis

- **README.md** - This file

## Benchmark Parameters

### --impl <asm|ref>
- `asm`: AArch64 assembly implementation with Crypto Extensions
- `ref`: C reference implementation (intrinsics)

### --op <enc|dec>
- `enc`: Encryption
- `dec`: Decryption

### --ks <128|192|256>
- Key size in bits

### --bytes <size>
- Total bytes to process
- Supports suffixes: K/Ki (kilobytes), M/Mi (megabytes), G/Gi (gigabytes)
- Examples: `1Gi` (1 GiB), `512Mi` (512 MiB), `2G` (2 GB)

### --batch <1|4|8|16>
- Batch factor: how many 16-block calls to group together
- `batch=1`: 256 bytes per iteration (16 blocks × 16 bytes)
- `batch=16`: 4096 bytes per iteration (256 blocks × 16 bytes) - optimal throughput

### --warm <N>
- Number of warmup iterations before timing (default: 2)

## Optimal Test Configuration

For best performance measurements:

1. **Set CPU governor to performance:**
   ```bash
   sudo cpupower frequency-set -g performance
   ```

2. **Pin to a big core (on big.LITTLE systems):**
   ```bash
   # Find big core IDs
   cat /proc/cpuinfo | grep "CPU part"

   # Run pinned to core 7 (example)
   taskset -c 7 ./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
   ```

3. **Use large dataset for throughput:**
   - Minimum: 512 MiB
   - Recommended: 2 GiB
   - Larger datasets amortize setup overhead

4. **Run multiple times and take median:**
   ```bash
   for i in {1..5}; do
       ./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
   done
   ```

## Performance Analysis

### Throughput Metrics

The benchmark reports two throughput metrics:
- **MiB/s** (mebibytes/sec): Binary units (1 MiB = 1024² bytes)
- **MB/s** (megabytes/sec): Decimal units (1 MB = 10⁶ bytes)

### Cycles per Byte

Use `perf stat` to measure cycles per byte:

```bash
perf stat -r 5 -e cycles,instructions \
    ./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16

# Calculate: cycles/byte = (total cycles) / (2147483648 bytes)
```

### Cache Analysis

```bash
perf stat -r 5 \
    -e L1-dcache-loads,L1-dcache-load-misses,LLC-load-misses \
    ./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
```

## Example Output

```
ASM  ENC  ks=128 batch=16  bytes= 2147483648  time=4.123456 s
  throughput:   496.234 MiB/s,   520.456 MB/s
```

This indicates:
- Implementation: Assembly (ASM)
- Operation: Encryption (ENC)
- Key size: 128 bits
- Batch size: 16 (4096 bytes per iteration)
- Total bytes processed: 2 GiB
- Time taken: 4.12 seconds
- Throughput: 496 MiB/s or 520 MB/s

## Interpreting Results

### Good Performance Indicators
- ASM faster than REF by 1.5-3× (depends on CPU)
- High IPC (instructions per cycle) > 2.0
- Low L1 cache miss rate < 5%
- Throughput scales well with batch size (16 > 8 > 4 > 1)

### Performance Issues
- ASM slower than REF: Check for:
  - Incorrect CPU frequency (not in performance mode)
  - Thermal throttling
  - Background processes
  - Suboptimal instruction scheduling in assembly

- Low throughput: Check for:
  - Small dataset (< 512 MiB) - increase to 2 GiB
  - Running on LITTLE core (big.LITTLE systems)
  - Memory bandwidth saturation

## Troubleshooting

### Compilation Errors

```bash
# Ensure you have ARMv8 Crypto Extension support
gcc -march=armv8-a+crypto -dM -E - < /dev/null | grep -i aes

# Should show __ARM_FEATURE_AES and __ARM_FEATURE_CRYPTO
```

### Runtime Errors

```bash
# Illegal instruction: CPU doesn't support crypto extensions
# Check CPU features
cat /proc/cpuinfo | grep Features
# Should include: aes pmull sha1 sha2

# If missing, you're on a CPU without ARMv8 Crypto Extension
# The assembly code will not work
```

### Unexpected Results

1. **REF faster than ASM on small datasets:**
   - Normal - ABI overhead (q8-q15 save/restore) more visible
   - Use larger dataset (2 GiB) for fair comparison

2. **Huge performance variance:**
   - Set performance governor
   - Pin to single core
   - Close background processes
   - Disable CPU frequency scaling

3. **Very low throughput:**
   - Check if running on LITTLE core
   - Verify crypto extensions enabled (`-march=armv8-a+crypto`)
   - Check for thermal throttling

## Next Steps

After collecting benchmark data:

1. **Document results in ../BENCHMARK.md**
2. **Compare with other implementations** (OpenSSL, reference)
3. **Identify optimization opportunities** (prefetch, scheduling, etc.)
4. **Test on different ARM cores** (Graviton, Altra, Apple Silicon)

## References

- [AAPCS64 ABI](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)
- [ARMv8 Crypto Extension](https://developer.arm.com/architectures/instruction-sets/intrinsics/#f:@navigationhierarchieselementbitsize=[128]&q=aes)
- [perf Linux profiler](https://perf.wiki.kernel.org/index.php/Main_Page)

# Camellia AArch64 Benchmark Makefile
# Include this in your main Makefile or use directly: make -f bench.mk

# Compiler settings
CC      ?= gcc
CFLAGS  ?= -O3 -DNDEBUG -Wall -Wextra
LDFLAGS ?=

# AArch64-specific flags
A64_FLAGS = -march=armv8-a+crypto -mtune=native

# Source files
ASM_SRC = camellia_simd128_aarch64_ce.S
REF_C   = camellia_simd128_with_aes_instruction_set.c
BENCH_C = tools/bench_camellia_a64.c

# Output binary
BENCH_BIN = bench_camellia_a64

.PHONY: all bench clean run-bench perf-bench help

all: bench

# Build benchmark binary
bench: $(BENCH_BIN)

$(BENCH_BIN): $(BENCH_C) $(ASM_SRC) $(REF_C)
	$(CC) $(CFLAGS) $(A64_FLAGS) -o $@ $(BENCH_C) $(ASM_SRC) -lm
	@echo ""
	@echo "✅ Benchmark binary built: ./$(BENCH_BIN)"
	@echo ""
	@echo "Quick start:"
	@echo "  ./$(BENCH_BIN) --help"
	@echo "  ./$(BENCH_BIN) --impl asm --op enc --ks 128 --bytes 2Gi --batch 16"
	@echo ""

# Quick test run
run-bench: $(BENCH_BIN)
	@echo "========================================="
	@echo "  Quick Benchmark (small dataset)"
	@echo "========================================="
	@echo ""
	@echo "== Warmup with C reference =="
	./$(BENCH_BIN) --impl ref --op enc --ks 128 --bytes 256Mi --batch 16
	@echo ""
	@echo "== AArch64 Assembly - Encryption =="
	./$(BENCH_BIN) --impl asm --op enc --ks 128 --bytes 1Gi --batch 16
	@echo ""
	@echo "== AArch64 Assembly - Decryption =="
	./$(BENCH_BIN) --impl asm --op dec --ks 128 --bytes 1Gi --batch 16
	@echo ""

# Full benchmark matrix (all key sizes, operations, batch sizes)
full-bench: $(BENCH_BIN)
	@echo "========================================="
	@echo "  Full Benchmark Matrix"
	@echo "========================================="
	@echo ""
	@./tools/bench_all_a64.sh

# Performance analysis with perf (requires Linux perf tools)
perf-bench: $(BENCH_BIN)
	@echo "========================================="
	@echo "  Performance Analysis with perf"
	@echo "========================================="
	@echo ""
	@echo "Note: This requires 'perf' to be installed and may need root/CAP_PERFMON"
	@echo ""
	perf stat -r 5 \
		-e cycles,instructions,task-clock,branches,branch-misses,\
		L1-dcache-loads,L1-dcache-load-misses,LLC-load-misses \
		./$(BENCH_BIN) --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
	@echo ""
	@echo "To calculate cycles/byte: (total cycles) / (bytes processed)"
	@echo ""

# Clean build artifacts
clean:
	rm -f $(BENCH_BIN) *.o

help:
	@echo "Camellia AArch64 Benchmark Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make bench        - Build benchmark binary"
	@echo "  make run-bench    - Run quick benchmark"
	@echo "  make full-bench   - Run full benchmark matrix (all configs)"
	@echo "  make perf-bench   - Run with perf stat for detailed metrics"
	@echo "  make clean        - Remove build artifacts"
	@echo ""
	@echo "Environment variables:"
	@echo "  CC        - C compiler (default: gcc)"
	@echo "  CFLAGS    - Compiler flags (default: -O3 -DNDEBUG -Wall)"
	@echo "  A64_FLAGS - AArch64 flags (default: -march=armv8-a+crypto -mtune=native)"
	@echo ""
	@echo "Examples:"
	@echo "  make bench"
	@echo "  make run-bench"
	@echo "  taskset -c 7 make run-bench    # Pin to CPU core 7"
	@echo ""
	@echo "For best results:"
	@echo "  1. Set CPU governor to 'performance':"
	@echo "     sudo cpupower frequency-set -g performance"
	@echo "  2. Pin to a big core (on big.LITTLE systems):"
	@echo "     taskset -c <big-core-id> ./$(BENCH_BIN) ..."
	@echo "  3. Disable turbo/boost for stable measurements"
	@echo ""

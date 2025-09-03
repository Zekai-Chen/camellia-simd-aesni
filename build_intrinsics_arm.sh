#!/bin/bash
# ARM native build script for intrinsics comparison test
# Run this directly on ARM/AArch64 machines

echo "🚀 Building AArch64 Intrinsics vs Assembly Test (ARM Native)"
echo "============================================================"

# Check if we're on ARM
if [ "$(uname -m)" != "aarch64" ]; then
    echo "❌ This script is designed for ARM64/AArch64 machines only"
    echo "   Current architecture: $(uname -m)"
    echo "   Please run this on an ARM machine (AWS Graviton, Apple Silicon, etc.)"
    exit 1
fi

echo "✅ Confirmed ARM64 architecture: $(uname -m)"

# Clean up
echo "🧹 Cleaning up old files..."
rm -f *.o test_aarch64_intrinsics test_aarch64_intrinsics_optimized

# Auto-detect ARM CPU for optimal compilation
detect_arm_cpu() {
    if grep -q "Neoverse-V1" /proc/cpuinfo 2>/dev/null; then
        echo "neoverse-v1+crypto"
    elif grep -q "Neoverse-N1" /proc/cpuinfo 2>/dev/null; then
        echo "neoverse-n1+crypto" 
    elif grep -q "Cortex-A72" /proc/cpuinfo 2>/dev/null; then
        echo "cortex-a72+crypto"
    elif grep -q "Apple" /proc/cpuinfo 2>/dev/null; then
        echo "apple-m1+crypto"
    else
        echo "armv8-a+crypto"  # Generic fallback
    fi
}

CPU_TARGET=$(detect_arm_cpu)
echo "🎯 Detected ARM CPU target: $CPU_TARGET"
echo ""

# Method 1: Standard compilation
echo "📦 Method 1: Standard compilation"
echo "---------------------------------"

echo "🔧 Compiling C test code..."
gcc -O2 -static -Wall -march=$CPU_TARGET \
    -c test_aarch64_intrinsics.c -o test_main.o

if [ $? -ne 0 ]; then
    echo "❌ C compilation failed"
    exit 1
fi

echo "🔧 Assembling SIMD code..."
gcc -march=$CPU_TARGET -c camellia_aarch64_neon_crypto.S -o camellia_simd.o

if [ $? -ne 0 ]; then
    echo "❌ Assembly failed"
    exit 1
fi

echo "🔗 Linking..."
gcc -O2 -static test_main.o camellia_simd.o -o test_aarch64_intrinsics

if [ $? -eq 0 ]; then
    echo "✅ Standard build successful: test_aarch64_intrinsics"
else
    echo "❌ Linking failed"
    exit 1
fi

echo ""

# Method 2: Optimized compilation (separate C and assembly optimization)
echo "📦 Method 2: Optimized compilation"
echo "----------------------------------"

echo "🔧 Compiling C code with conservative flags..."
gcc -O1 -static -Wall -march=$CPU_TARGET -fno-unroll-loops -fno-tree-vectorize -fno-builtin \
    -fwrapv -fno-strict-aliasing -fno-inline -fno-omit-frame-pointer \
    -c test_aarch64_intrinsics.c -o test_main_opt.o

if [ $? -ne 0 ]; then
    echo "❌ Optimized C compilation failed"
    exit 1
fi

echo "🔧 Assembling SIMD with target optimization..."  
gcc -march=$CPU_TARGET -c camellia_aarch64_neon_crypto.S -o camellia_simd_opt.o

if [ $? -ne 0 ]; then
    echo "❌ Optimized assembly failed"
    exit 1
fi

echo "🔗 Linking optimized version..."
gcc -O1 -static test_main_opt.o camellia_simd_opt.o -o test_aarch64_intrinsics_optimized

if [ $? -eq 0 ]; then
    echo "✅ Optimized build successful: test_aarch64_intrinsics_optimized"
else
    echo "❌ Optimized linking failed"
    exit 1
fi

echo ""
echo "🎉 Build Complete!"
echo "=================="
echo ""
echo "Available test executables:"
echo "  📊 test_aarch64_intrinsics           - Standard compilation"  
echo "  🚀 test_aarch64_intrinsics_optimized - Optimized compilation (recommended)"
echo ""
echo "Usage:"
echo "  # Standard test"
echo "  ./test_aarch64_intrinsics"
echo ""
echo "  # Optimized test with CPU binding (most stable results)"
echo "  taskset -c 1 ./test_aarch64_intrinsics_optimized"
echo ""
echo "📊 What to expect:"
echo "  - C Reference: ~70-150 MB/s (baseline)"
echo "  - C Intrinsics: 70-90% of assembly performance"  
echo "  - Hand Assembly: Best performance (~450 MB/s on Graviton3)"
echo "  - Assembly should slightly outperform intrinsics"
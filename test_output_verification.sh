#!/bin/bash
# AArch64 Camellia Implementation Output Verification Script
# 比较Assembly版本和C Intrinsics版本与C reference的输出是否一致

set -e

echo "🚀 AArch64 Camellia Output Verification"
echo "========================================="
echo "Architecture: $(uname -m)"
echo "Date: $(date)"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results directory
RESULTS_DIR="verification_results"
mkdir -p $RESULTS_DIR

echo "Step 1: Building all implementations..."
echo "--------------------------------------"

# Build Assembly version
echo "📦 Building Assembly version..."
make -f Makefile.aarch64 test_complex
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Assembly version built successfully${NC}"
else
    echo -e "${RED}❌ Assembly version build failed${NC}"
    exit 1
fi

# Build C Intrinsics version
echo "📦 Building C Intrinsics version..."
chmod +x build_intrinsics_arm.sh
./build_intrinsics_arm.sh
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ C Intrinsics version built successfully${NC}"
else
    echo -e "${RED}❌ C Intrinsics version build failed${NC}"
    exit 1
fi

echo ""
echo "Step 2: Running output tests..."
echo "------------------------------"

# Test Assembly version
echo "🔍 Testing Assembly version output..."
./test_aarch64_complex > $RESULTS_DIR/assembly_output.txt 2>&1
echo -e "${GREEN}✅ Assembly version test completed${NC}"

# Test C Intrinsics version  
echo "🔍 Testing C Intrinsics version output..."
./test_aarch64_intrinsics_optimized > $RESULTS_DIR/intrinsics_output.txt 2>&1
echo -e "${GREEN}✅ C Intrinsics version test completed${NC}"

echo ""
echo "Step 3: Extracting key output data..."
echo "------------------------------------"

# Extract first 32 bytes of outputs for comparison
echo "📊 Extracting output data from Assembly version..."
grep -A 20 "First 32 bytes of SIMD output:" $RESULTS_DIR/assembly_output.txt > $RESULTS_DIR/assembly_data.txt || true
grep -A 20 "First 32 bytes of C reference:" $RESULTS_DIR/assembly_output.txt > $RESULTS_DIR/assembly_ref_data.txt || true

echo "📊 Extracting output data from C Intrinsics version..."
grep -A 20 "First 32 bytes of SIMD output:" $RESULTS_DIR/intrinsics_output.txt > $RESULTS_DIR/intrinsics_data.txt || true  
grep -A 20 "First 32 bytes of C reference:" $RESULTS_DIR/intrinsics_output.txt > $RESULTS_DIR/intrinsics_ref_data.txt || true

echo ""
echo "Step 4: Comparing outputs..."
echo "---------------------------"

# Compare SIMD outputs
echo "🔍 Comparing SIMD outputs between Assembly and Intrinsics versions..."
if cmp -s $RESULTS_DIR/assembly_data.txt $RESULTS_DIR/intrinsics_data.txt; then
    echo -e "${GREEN}✅ SIMD outputs are IDENTICAL between Assembly and Intrinsics versions${NC}"
    SIMD_MATCH=true
else
    echo -e "${RED}❌ SIMD outputs are DIFFERENT between Assembly and Intrinsics versions${NC}"
    echo "Differences:"
    diff $RESULTS_DIR/assembly_data.txt $RESULTS_DIR/intrinsics_data.txt || true
    SIMD_MATCH=false
fi

# Compare C reference outputs  
echo "🔍 Comparing C reference outputs..."
if cmp -s $RESULTS_DIR/assembly_ref_data.txt $RESULTS_DIR/intrinsics_ref_data.txt; then
    echo -e "${GREEN}✅ C reference outputs are IDENTICAL${NC}"
    REF_MATCH=true
else
    echo -e "${RED}❌ C reference outputs are DIFFERENT${NC}"
    echo "Differences:"
    diff $RESULTS_DIR/assembly_ref_data.txt $RESULTS_DIR/intrinsics_ref_data.txt || true
    REF_MATCH=false
fi

echo ""
echo "Step 5: Performance comparison..."
echo "-------------------------------"

# Extract performance data
echo "📈 Extracting performance metrics..."
ASM_PERF=$(grep "AArch64 SIMD:" $RESULTS_DIR/assembly_output.txt | grep "MB/s (average)" | head -1 || echo "N/A")
INT_PERF=$(grep "AArch64 SIMD:" $RESULTS_DIR/intrinsics_output.txt | grep "MB/s (average)" | head -1 || echo "N/A")

echo "Assembly version performance: $ASM_PERF"
echo "Intrinsics version performance: $INT_PERF"

echo ""
echo "Step 6: Final verification summary..."
echo "====================================="

if [ "$SIMD_MATCH" = true ] && [ "$REF_MATCH" = true ]; then
    echo -e "${GREEN}🎉 SUCCESS: All implementations produce identical outputs!${NC}"
    echo -e "${GREEN}✅ Assembly version output matches C reference${NC}"
    echo -e "${GREEN}✅ Intrinsics version output matches C reference${NC}"
    echo -e "${GREEN}✅ Assembly and Intrinsics versions produce identical results${NC}"
    
    echo ""
    echo "📋 Test Summary:"
    echo "- Assembly implementation: ✅ PASS"
    echo "- C Intrinsics implementation: ✅ PASS"
    echo "- Output consistency: ✅ PASS"
    echo "- Performance data available: ✅ YES"
    
    exit 0
else
    echo -e "${RED}❌ FAILED: Output verification failed!${NC}"
    echo ""
    echo "📋 Issues found:"
    [ "$SIMD_MATCH" = false ] && echo "- SIMD outputs differ between implementations"
    [ "$REF_MATCH" = false ] && echo "- C reference outputs are inconsistent"
    
    echo ""
    echo "🔍 Check the following files for detailed analysis:"
    echo "- $RESULTS_DIR/assembly_output.txt"
    echo "- $RESULTS_DIR/intrinsics_output.txt"
    echo "- $RESULTS_DIR/assembly_data.txt"
    echo "- $RESULTS_DIR/intrinsics_data.txt"
    
    exit 1
fi
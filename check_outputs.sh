#!/bin/bash
# 直接检查Assembly和Intrinsics版本的输出差异

echo "🔍 Direct Output Analysis"
echo "========================"

echo "1. Running Assembly version..."
echo "------------------------------"
./test_aarch64_complex | grep -A 5 -B 2 "First 32 bytes" > assembly_check.txt
cat assembly_check.txt
echo ""

echo "2. Running C Intrinsics version..."
echo "----------------------------------"
taskset -c 1 ./test_aarch64_intrinsics_optimized | grep -A 5 -B 2 "First 32 bytes" > intrinsics_check.txt
cat intrinsics_check.txt
echo ""

echo "3. Detailed Comparison..."
echo "------------------------"
echo "Assembly SIMD output:"
grep "First 32 bytes of SIMD output:" assembly_check.txt -A 2

echo ""
echo "Assembly C Reference output:"
grep "First 32 bytes of C reference:" assembly_check.txt -A 2

echo ""
echo "Intrinsics SIMD output:"
grep "First 32 bytes of SIMD output:" intrinsics_check.txt -A 2

echo ""
echo "Intrinsics C Reference output:"
grep "First 32 bytes of C reference:" intrinsics_check.txt -A 2

echo ""
echo "4. Analysis Summary..."
echo "---------------------"

# Extract hex values for comparison
ASM_SIMD=$(grep -A 2 "First 32 bytes of SIMD output:" assembly_check.txt | grep -E "^[0-9a-f]" | head -1)
ASM_REF=$(grep -A 2 "First 32 bytes of C reference:" assembly_check.txt | grep -E "^[0-9a-f]" | head -1)
INT_SIMD=$(grep -A 2 "First 32 bytes of SIMD output:" intrinsics_check.txt | grep -E "^[0-9a-f]" | head -1)
INT_REF=$(grep -A 2 "First 32 bytes of C reference:" intrinsics_check.txt | grep -E "^[0-9a-f]" | head -1)

echo "Assembly SIMD:      $ASM_SIMD"
echo "Assembly C Ref:     $ASM_REF"
echo "Intrinsics SIMD:    $INT_SIMD"  
echo "Intrinsics C Ref:   $INT_REF"

echo ""
if [ "$ASM_SIMD" = "$ASM_REF" ]; then
    echo "✅ Assembly: SIMD matches C Reference"
else
    echo "❌ Assembly: SIMD differs from C Reference"
fi

if [ "$INT_SIMD" = "$INT_REF" ]; then
    echo "✅ Intrinsics: SIMD matches C Reference"
else
    echo "❌ Intrinsics: SIMD differs from C Reference"
fi

if [ "$ASM_SIMD" = "$INT_SIMD" ]; then
    echo "✅ Both SIMD implementations produce same result"
else
    echo "❌ SIMD implementations produce different results"
fi

if [ "$ASM_REF" = "$INT_REF" ]; then
    echo "✅ Both C References produce same result"
else
    echo "❌ C References produce different results (this shouldn't happen)"
fi
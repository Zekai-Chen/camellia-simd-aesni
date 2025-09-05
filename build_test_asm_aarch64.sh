#!/bin/bash
#
# Build and test script for Camellia AArch64 Assembly implementation
#

echo "========================================"
echo "Building Camellia AArch64 Assembly Implementation"
echo "========================================"
echo

# Clean previous builds
echo "Cleaning previous builds..."
make clean 2>/dev/null || true
rm -f test_simd128_asm_aarch64 *.o

# Build the assembly test program
echo "Building test_simd128_asm_aarch64..."
make test_simd128_asm_aarch64

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Run the test
echo
echo "========================================"
echo "Running Tests"
echo "========================================"
echo

./test_simd128_asm_aarch64

# Check test result
if [ $? -eq 0 ]; then
    echo
    echo "✓ All tests passed successfully!"
else
    echo
    echo "✗ Some tests failed."
fi

echo
echo "========================================"
echo "Build and Test Complete"
echo "========================================"
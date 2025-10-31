# Makefile for component tests
#
# 这些测试需要在AArch64平台上运行
# 如果在x86上编译，需要使用交叉编译器

# 检测架构
ARCH := $(shell uname -m)

ifeq ($(ARCH),aarch64)
    # 本地编译（在ARM机器上）
    CC = gcc
    CFLAGS = -Wall -g -O0 -march=armv8-a+crypto
else
    # 交叉编译（在x86上）
    CC = aarch64-linux-gnu-gcc
    CFLAGS = -Wall -g -O0 -march=armv8-a+crypto -static
endif

# 测试程序
TESTS = test_01_basic_macros \
        test_02_transpose \
        test_03_filter_8bit

all: $(TESTS)

test_01_basic_macros: test_01_basic_macros.c
	$(CC) $(CFLAGS) $< -o $@
	@echo "✓ Built $@"

test_02_transpose: test_02_transpose.c
	$(CC) $(CFLAGS) $< -o $@
	@echo "✓ Built $@"

test_03_filter_8bit: test_03_filter_8bit.c
	$(CC) $(CFLAGS) $< -o $@
	@echo "✓ Built $@"

# 运行所有测试（仅在AArch64上）
test: all
ifeq ($(ARCH),aarch64)
	@echo "========================================="
	@echo "Running all component tests..."
	@echo "========================================="
	@for test in $(TESTS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "========================================="
	@echo "✅ All component tests completed!"
	@echo "========================================="
else
	@echo "⚠️  Cannot run tests on $(ARCH)"
	@echo "   Please copy binaries to an AArch64 machine"
endif

clean:
	rm -f $(TESTS)

.PHONY: all test clean

#!/bin/bash

# Copyright (C) 2024 Camellia AArch64 SIMD Build and Test Script
# SPDX-License-Identifier: MIT

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
LOG_DIR="${SCRIPT_DIR}/logs"
RESULTS_DIR="${SCRIPT_DIR}/results"

# Create directories
mkdir -p "${BUILD_DIR}" "${LOG_DIR}" "${RESULTS_DIR}"

print_banner() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}  Camellia AArch64 SIMD Build and Test Suite${NC}"
    echo -e "${BLUE}================================================${NC}"
    echo ""
}

print_section() {
    echo -e "${YELLOW}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Detect system architecture and capabilities
detect_system() {
    print_section "System Detection"
    
    ARCH=$(uname -m)
    OS=$(uname -s)
    
    echo "Architecture: $ARCH"
    echo "Operating System: $OS"
    
    if [[ "$ARCH" != "aarch64" ]]; then
        print_error "This script is designed for AArch64 systems"
        echo "Current architecture: $ARCH"
        echo "Cross-compilation may be possible but is not configured"
        exit 1
    fi
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v gcc >/dev/null 2>&1; then
        missing_tools+=("gcc")
    fi
    
    if ! command -v make >/dev/null 2>&1; then
        missing_tools+=("make")
    fi
    
    if ! command -v pkg-config >/dev/null 2>&1; then
        missing_tools+=("pkg-config")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        print_error "Missing required tools: ${missing_tools[*]}"
        echo "Please install them using your package manager"
        exit 1
    fi
    
    # Check for OpenSSL development headers
    if pkg-config --exists openssl; then
        HAVE_OPENSSL=1
        print_success "OpenSSL development libraries found"
    else
        HAVE_OPENSSL=0
        print_info "OpenSSL development libraries not found - some benchmarks will be disabled"
    fi
    
    # Check hardware features
    if grep -q "^Features.*aes" /proc/cpuinfo; then
        HAVE_CRYPTO_EXT=1
        print_success "AArch64 Crypto Extensions detected"
    else
        HAVE_CRYPTO_EXT=0
        print_info "AArch64 Crypto Extensions not detected"
    fi
    
    if grep -q "^Features.*asimd" /proc/cpuinfo || grep -q "^Features.*neon" /proc/cpuinfo; then
        HAVE_NEON=1
        print_success "NEON SIMD support detected"
    else
        HAVE_NEON=0
        print_error "NEON SIMD support not detected"
    fi
    
    echo ""
}

# Clean previous builds
clean_build() {
    print_section "Cleaning Previous Build"
    
    cd "$SCRIPT_DIR"
    
    if [[ -f Makefile ]]; then
        make clean 2>/dev/null || true
    fi
    
    rm -rf "${BUILD_DIR}"/*
    rm -rf "${LOG_DIR}"/*
    
    print_success "Clean completed"
    echo ""
}

# Build all targets
build_targets() {
    print_section "Building Targets"
    
    cd "$SCRIPT_DIR"
    
    local build_flags=""
    if [[ $HAVE_OPENSSL -eq 1 ]]; then
        build_flags="$build_flags -DHAVE_OPENSSL"
    fi
    
    # Update CFLAGS in Makefile if needed
    export EXTRA_CFLAGS="$build_flags"
    
    local targets=(
        "test_simd128_intrinsics_aarch64"
        "simple_aarch64_test"
        "test_simd128_neon_aarch64" 
        "test_simd256_neon_aarch64"
        "test_camellia_benchmark_aarch64"
    )
    
    local built_targets=()
    local failed_targets=()
    
    for target in "${targets[@]}"; do
        echo "Building $target..."
        if make "$target" > "${LOG_DIR}/${target}_build.log" 2>&1; then
            print_success "Built $target"
            built_targets+=("$target")
        else
            print_error "Failed to build $target"
            failed_targets+=("$target")
            echo "  See ${LOG_DIR}/${target}_build.log for details"
        fi
    done
    
    echo ""
    echo "Build Summary:"
    echo "  Successful: ${#built_targets[@]}"
    echo "  Failed: ${#failed_targets[@]}"
    
    if [[ ${#failed_targets[@]} -gt 0 ]]; then
        echo "  Failed targets: ${failed_targets[*]}"
    fi
    
    echo ""
    
    # Return list of successfully built targets
    BUILT_TARGETS=("${built_targets[@]}")
}

# Run tests
run_tests() {
    print_section "Running Tests"
    
    if [[ ${#BUILT_TARGETS[@]} -eq 0 ]]; then
        print_error "No targets were built successfully"
        return 1
    fi
    
    cd "$SCRIPT_DIR"
    
    local test_results=()
    
    for target in "${BUILT_TARGETS[@]}"; do
        if [[ ! -x "./$target" ]]; then
            print_error "$target is not executable"
            continue
        fi
        
        echo "Running $target..."
        
        local log_file="${LOG_DIR}/${target}_test.log"
        local result_file="${RESULTS_DIR}/${target}_results.txt"
        
        if timeout 300 "./$target" > "$result_file" 2>&1; then
            print_success "$target completed"
            test_results+=("$target:PASS")
        else
            local exit_code=$?
            if [[ $exit_code -eq 124 ]]; then
                print_error "$target timed out"
                test_results+=("$target:TIMEOUT")
            else
                print_error "$target failed (exit code: $exit_code)"
                test_results+=("$target:FAIL")
            fi
            cp "$result_file" "$log_file"
        fi
    done
    
    echo ""
    echo "Test Summary:"
    for result in "${test_results[@]}"; do
        local target="${result%:*}"
        local status="${result#*:}"
        case "$status" in
            "PASS") print_success "$target" ;;
            "FAIL") print_error "$target" ;;
            "TIMEOUT") print_error "$target (timeout)" ;;
        esac
    done
    
    echo ""
}

# Performance benchmarks
run_benchmarks() {
    print_section "Performance Benchmarks"
    
    if [[ ! -x "./test_camellia_benchmark_aarch64" ]]; then
        print_error "Benchmark executable not available"
        return 1
    fi
    
    print_info "Running comprehensive performance benchmarks..."
    print_info "This may take several minutes..."
    
    local benchmark_file="${RESULTS_DIR}/performance_benchmark.txt"
    local perf_file="${RESULTS_DIR}/perf_analysis.txt"
    
    # Run basic benchmark
    if "./test_camellia_benchmark_aarch64" > "$benchmark_file" 2>&1; then
        print_success "Basic benchmark completed"
        echo "Results saved to: $benchmark_file"
    else
        print_error "Basic benchmark failed"
    fi
    
    # Run perf analysis if available
    if command -v perf >/dev/null 2>&1; then
        print_info "Running perf analysis..."
        
        # Check if we can run perf without sudo
        if perf stat -e cycles,instructions echo "test" >/dev/null 2>&1; then
            if timeout 60 perf stat -e cycles,instructions,cache-misses,branch-misses \
                "./test_camellia_benchmark_aarch64" > "$perf_file" 2>&1; then
                print_success "Perf analysis completed"
                echo "Results saved to: $perf_file"
            else
                print_error "Perf analysis failed or timed out"
            fi
        else
            print_info "Perf requires elevated privileges - skipping detailed analysis"
        fi
    else
        print_info "Perf tool not available - skipping detailed analysis"
    fi
    
    echo ""
}

# Generate report
generate_report() {
    print_section "Generating Report"
    
    local report_file="${RESULTS_DIR}/test_report.html"
    local summary_file="${RESULTS_DIR}/summary.txt"
    
    # Generate summary
    {
        echo "Camellia AArch64 SIMD Test Report"
        echo "=================================="
        echo ""
        echo "Test Date: $(date)"
        echo "System: $(uname -a)"
        echo ""
        
        echo "Hardware Features:"
        echo "  NEON SIMD: $([[ $HAVE_NEON -eq 1 ]] && echo "Yes" || echo "No")"
        echo "  Crypto Extensions: $([[ $HAVE_CRYPTO_EXT -eq 1 ]] && echo "Yes" || echo "No")"
        echo ""
        
        echo "Build Results:"
        echo "  Targets Built: ${#BUILT_TARGETS[@]}"
        echo "  Built Targets: ${BUILT_TARGETS[*]}"
        echo ""
        
        if [[ -f "${RESULTS_DIR}/performance_benchmark.txt" ]]; then
            echo "Performance Results:"
            echo "==================="
            grep -E "(MiB/s|MB/s|cyc/byte)" "${RESULTS_DIR}/performance_benchmark.txt" | head -20
            echo ""
        fi
        
    } > "$summary_file"
    
    # Generate HTML report
    {
        cat << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Camellia AArch64 SIMD Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { background: #f0f8ff; padding: 20px; border-radius: 5px; }
        .section { margin: 20px 0; }
        .success { color: green; }
        .error { color: red; }
        .info { color: blue; }
        pre { background: #f5f5f5; padding: 15px; border-radius: 5px; overflow-x: auto; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Camellia AArch64 SIMD Test Report</h1>
EOF
        echo "        <p><strong>Generated:</strong> $(date)</p>"
        echo "        <p><strong>System:</strong> $(uname -a)</p>"
        echo "    </div>"
        
        echo "    <div class=\"section\">"
        echo "        <h2>System Information</h2>"
        echo "        <table>"
        echo "            <tr><td>Architecture</td><td>$ARCH</td></tr>"
        echo "            <tr><td>NEON Support</td><td>$([[ $HAVE_NEON -eq 1 ]] && echo '<span class="success">Yes</span>' || echo '<span class="error">No</span>')</td></tr>"
        echo "            <tr><td>Crypto Extensions</td><td>$([[ $HAVE_CRYPTO_EXT -eq 1 ]] && echo '<span class="success">Yes</span>' || echo '<span class="error">No</span>')</td></tr>"
        echo "            <tr><td>OpenSSL Dev</td><td>$([[ $HAVE_OPENSSL -eq 1 ]] && echo '<span class="success">Yes</span>' || echo '<span class="info">No</span>')</td></tr>"
        echo "        </table>"
        echo "    </div>"
        
        if [[ -f "${RESULTS_DIR}/performance_benchmark.txt" ]]; then
            echo "    <div class=\"section\">"
            echo "        <h2>Performance Results</h2>"
            echo "        <pre>"
            cat "${RESULTS_DIR}/performance_benchmark.txt"
            echo "        </pre>"
            echo "    </div>"
        fi
        
        echo "</body></html>"
        
    } > "$report_file"
    
    print_success "Report generated: $report_file"
    print_success "Summary generated: $summary_file"
    echo ""
}

# Main execution
main() {
    print_banner
    
    detect_system
    clean_build
    build_targets
    run_tests
    run_benchmarks
    generate_report
    
    print_section "Test Suite Complete"
    print_success "All tasks completed"
    echo ""
    echo "Results available in: $RESULTS_DIR"
    echo "Logs available in: $LOG_DIR"
    echo ""
    echo "Quick performance check:"
    if [[ -f "${RESULTS_DIR}/summary.txt" ]]; then
        tail -10 "${RESULTS_DIR}/summary.txt"
    fi
}

# Handle command line arguments
case "${1:-}" in
    "clean")
        clean_build
        ;;
    "build")
        detect_system
        build_targets
        ;;
    "test")
        detect_system
        run_tests
        ;;
    "benchmark")
        detect_system
        run_benchmarks
        ;;
    "report")
        generate_report
        ;;
    "help"|"-h"|"--help")
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  clean       Clean previous builds"
        echo "  build       Build all targets"
        echo "  test        Run tests"
        echo "  benchmark   Run performance benchmarks"
        echo "  report      Generate reports"
        echo "  help        Show this help"
        echo ""
        echo "If no command is specified, runs the complete test suite"
        ;;
    "")
        main
        ;;
    *)
        print_error "Unknown command: $1"
        echo "Use '$0 help' for usage information"
        exit 1
        ;;
esac
# GDB script for debugging Camellia encryption
# Usage: gdb -x debug.gdb ./test_diagnostic

# Set breakpoints
break camellia_encrypt_16blks_simd128_aarch64_asm
break roundsm16

# Run the program
run

# When we hit the first breakpoint (main encryption function)
commands 1
  printf "=== Entering Assembly encryption ===\n"
  printf "ctx pointer: %p\n", $x0
  printf "output pointer: %p\n", $x1
  printf "input pointer: %p\n", $x2
  continue
end

# When we hit roundsm16
commands 2
  printf "\n=== Entering roundsm16 ===\n"
  printf "key_ptr (x10): %p\n", $x10
  printf "cd_ptr (x11): %p\n", $x11
  printf "AB state v0-v3:\n"
  printf "  v0: "
  x/16bx $v0
  printf "  v1: "
  x/16bx $v1
  printf "  v2: "
  x/16bx $v2
  printf "  v3: "
  x/16bx $v3
  continue
end

# Continue execution
continue

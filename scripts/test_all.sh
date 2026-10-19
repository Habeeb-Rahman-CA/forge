#!/usr/bin/env bash
# Project Forge — Unified Automated Test Runner
# Executes C unit tests, end-to-end CLI program tests, and Angular frontend tests.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "========================================================================"
echo "          Project Forge — Complete System Test Suite Runner"
echo "========================================================================"

cd "${ROOT_DIR}"

# 1. Check or build C project with CMake
echo ""
echo "[Step 1/3] Building C tools and running CTest unit test suites..."
if [ ! -d "build" ]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build --parallel

# Run CTest
ctest --test-dir build --output-on-failure
echo "✓ All C unit tests passed (Emulator, Assembler, Compiler, Bridge)."

# 2. Integration: Compile and run all example assembly and ForgeLang programs
echo ""
echo "[Step 2/3] Running End-to-End Example Program Verification..."
mkdir -p /tmp/forge_test_bins

# Test Assembly Examples
for asm_file in examples/assembly/*.asm; do
    base_name=$(basename "${asm_file}" .asm)
    bin_file="/tmp/forge_test_bins/${base_name}.fbin"
    echo -n "  • Assembling and executing ${asm_file}... "
    ./build/assembler/forge-asm "${asm_file}" -o "${bin_file}" > /dev/null
    ./build/emulator/forge-emu "${bin_file}" > /dev/null
    echo "PASS"
done

# Test ForgeLang Examples
for fl_file in examples/programs/*.fl; do
    echo -n "  • Compiling and running ${fl_file}... "
    ./build/compiler/forge-cc "${fl_file}" --run > /dev/null
    echo "PASS"
done
echo "✓ All example programs assembled, compiled, and executed successfully."

# 3. Frontend Unit & Integration Tests (Angular 21 + Vitest)
echo ""
echo "[Step 3/3] Running Angular Frontend Vitest test suites..."
npm --prefix frontend test

echo ""
echo "========================================================================"
echo "   🎉 ALL 100% OF TESTS PASSED ACROSS THE ENTIRE FORGE PLATFORM!"
echo "========================================================================"


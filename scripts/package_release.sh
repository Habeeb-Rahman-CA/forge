#!/usr/bin/env bash
# Project Forge — Release Packaging Script
# Builds Release C binaries, production frontend bundle, pre-assembles examples,
# and generates the distributable archive: dist/forge-v1.0.0-linux-x86_64.tar.gz

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

VERSION="1.0.0"
PLATFORM="linux-x86_64"
RELEASE_NAME="forge-v${VERSION}-${PLATFORM}"
DIST_DIR="${ROOT_DIR}/dist"
STAGE_DIR="${DIST_DIR}/${RELEASE_NAME}"

echo "========================================================================"
echo "          Project Forge — Packaging Production Release v${VERSION}"
echo "========================================================================"

cd "${ROOT_DIR}"

# Clean previous distribution directory
rm -rf "${STAGE_DIR}" "${DIST_DIR}/${RELEASE_NAME}.tar.gz"
mkdir -p "${STAGE_DIR}/bin" "${STAGE_DIR}/include" "${STAGE_DIR}/docs" "${STAGE_DIR}/examples" "${STAGE_DIR}/web"

# 1. Compile C Binaries in Release Mode (-O3)
echo ""
echo "[1/5] Compiling optimized Release C binaries..."
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel

# Verify with CTest
echo "[1.1/5] Verifying CTest test suites..."
ctest --test-dir build-release --output-on-failure

# Copy C Executables
cp build-release/emulator/forge-emu "${STAGE_DIR}/bin/"
cp build-release/assembler/forge-asm "${STAGE_DIR}/bin/"
cp build-release/compiler/forge-cc "${STAGE_DIR}/bin/"
cp build-release/assembler/forge-bridge "${STAGE_DIR}/bin/"
chmod +x "${STAGE_DIR}/bin/"*

# 2. Build Production Angular Frontend
echo ""
echo "[2/5] Building production Angular Web Debugger..."
npm --prefix frontend run build:prod
cp -r frontend/dist/forge-ui/browser/* "${STAGE_DIR}/web/"

# 3. Pre-assemble Example Programs into Executable .fbin Binaries
echo ""
echo "[3/5] Pre-assembling example programs into .fbin binaries..."
mkdir -p "${STAGE_DIR}/examples/assembly" "${STAGE_DIR}/examples/programs" "${STAGE_DIR}/examples/binaries"
cp examples/assembly/*.asm "${STAGE_DIR}/examples/assembly/"
cp examples/programs/*.fl "${STAGE_DIR}/examples/programs/"

for asm_file in examples/assembly/*.asm; do
    base_name=$(basename "${asm_file}" .asm)
    "${STAGE_DIR}/bin/forge-asm" "${asm_file}" -o "${STAGE_DIR}/examples/binaries/${base_name}.fbin"
done

for fl_file in examples/programs/*.fl; do
    base_name=$(basename "${fl_file}" .fl)
    "${STAGE_DIR}/bin/forge-cc" "${fl_file}" -o "${STAGE_DIR}/examples/binaries/${base_name}.fbin"
done
echo "✓ Pre-compiled $(ls -1 ${STAGE_DIR}/examples/binaries/*.fbin | wc -l) binary executables."

# 4. Copy Public Headers, Documentation & Licenses
echo ""
echo "[4/5] Copying architecture documentation & public C headers..."
cp docs/*.md "${STAGE_DIR}/docs/"
cp emulator/include/*.h "${STAGE_DIR}/include/"
cp assembler/include/*.h "${STAGE_DIR}/include/"
cp compiler/include/*.h "${STAGE_DIR}/include/"
cp README.md "${STAGE_DIR}/"

# Create all-in-one launcher script
cat << 'EOF' > "${STAGE_DIR}/bin/forge-lab.sh"
#!/usr/bin/env bash
# Forge-16 Laboratory All-in-One Launcher
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Starting Forge-16 Native C HTTP/JSON Bridge on 127.0.0.1:8080..."
"${DIR}/bin/forge-bridge" 8080 &
BRIDGE_PID=$!

trap "kill ${BRIDGE_PID} 2>/dev/null || true" EXIT INT TERM

echo ""
echo "========================================================================"
echo "    Forge-16 Systems Laboratory is now running!"
echo "    - Native C Bridge: http://127.0.0.1:8080"
echo "    - Web Debugger:    file://${DIR}/web/index.html"
echo "========================================================================"
echo "Press Ctrl+C to terminate."
wait ${BRIDGE_PID}
EOF
chmod +x "${STAGE_DIR}/bin/forge-lab.sh"

# 5. Create Compressed Tarball
echo ""
echo "[5/5] Creating compressed release archive..."
cd "${DIST_DIR}"
tar -czf "${RELEASE_NAME}.tar.gz" "${RELEASE_NAME}"

ARCHIVE_PATH="${DIST_DIR}/${RELEASE_NAME}.tar.gz"
ARCHIVE_SIZE=$(du -h "${ARCHIVE_PATH}" | cut -f1)

echo ""
echo "========================================================================"
echo "  🎉 Production Release Package Ready!"
echo "  Archive: ${ARCHIVE_PATH} (${ARCHIVE_SIZE})"
echo "========================================================================"
ls -lh "${ARCHIVE_PATH}"


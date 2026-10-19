#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[Forge] Building C Emulator, Assembler, and Native Bridge..."
cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build"
cmake --build "${ROOT_DIR}/build"

echo "[Forge] Starting Native C Bridge on http://127.0.0.1:8080 ..."
"${ROOT_DIR}/build/assembler/forge-bridge" --port 8080 &
BRIDGE_PID=$!
trap 'kill ${BRIDGE_PID} 2>/dev/null || true' EXIT

echo "[Forge] Starting Angular Visual Debugger on http://localhost:4200 ..."
cd "${ROOT_DIR}/frontend"
HOME=/tmp npm start


#!/usr/bin/env bash
set -eu

# ——————— Configuration ———————
REMOTE_DIR="backend/test_data/test_dir"
PORT=8000
URL="http://127.0.0.1:${PORT}/api/data"
CACHE_DIR="mnt/cache"
MOUNT_POINT="mnt/fuse_test"
REMOTE_CACHE="./remote_cache"
# ————————————————————————————

cleanup() {
  echo "Cleaning up…"
  kill "${SERVER_PID:-}" "${MOUNT_PID:-}" &>/dev/null || true
  fusermount3 -u "${MOUNT_POINT}" &>/dev/null || true
  rm -rf "${CACHE_DIR}" "${MOUNT_POINT}"
}

# prepare
rm -rf "${CACHE_DIR}" "${MOUNT_POINT}"
mkdir -p "${CACHE_DIR}" "${MOUNT_POINT}"

# 1) Start backend
python3 backend/local_server.py \
  --port "${PORT}" \
  --directory "${PWD}/${REMOTE_DIR}" &
SERVER_PID=$!

sleep 1

# 2) Mount FUSE
"${REMOTE_CACHE}" \
  "${CACHE_DIR}" \
  "${URL}" \
  "${MOUNT_POINT}" \
  -s -f -d &
MOUNT_PID=$!

sleep 1

echo "=== test_fuse ==="

# 3) Check file existence
if [ ! -f "${MOUNT_POINT}/file_0.txt" ]; then
  echo "FAIL: file_0.txt not found in mount"
  cleanup
  exit 1
else
  echo "PASS: file_0.txt found in mount"
fi

# 4) Check contents match
REMOTE_CONTENT=$(< "${REMOTE_DIR}/file_0.txt")
MOUNT_CONTENT=$(< "${MOUNT_POINT}/file_0.txt")

if [ "${MOUNT_CONTENT}" = "${REMOTE_CONTENT}" ]; then
  echo "PASS: file_0.txt content matches remote"
else
  echo "FAIL: file_0.txt content mismatch"
  cleanup
  exit 1
fi

# 5) Write-through test
echo "smoke test" > "${MOUNT_POINT}/smoke.txt"
sleep 0.5

if grep -q "smoke test" "${REMOTE_DIR}/smoke.txt"; then
  echo "PASS: write-through smoke.txt"
else
  echo "FAIL: smoke.txt not written through to remote store"
  cleanup
  exit 1
fi

echo "All tests passed."
echo "FUSE is mounted at ${MOUNT_POINT}"
echo "You can now run:"
echo "    python3 fuse_latency_test_plot.py"
echo

read -p "Press Enter to unmount and clean up..."

cleanup


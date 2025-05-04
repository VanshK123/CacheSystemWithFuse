#!/usr/bin/env bash
set -eu

# ——————— Configuration ———————
REMOTE_DIR="backend/test_data/test_dir"
PORT=8000

# *** POINT AT /api/data so that GET and PUT both hit the data endpoint ***
URL="http://127.0.0.1:${PORT}/api/data"

CACHE_DIR="mnt/cache"
MOUNT_POINT="mnt/fuse_test"

# path to your FUSE client binary
REMOTE_CACHE="./remote_cache"
# ————————————————————————————

cleanup() {
  echo "Cleaning up…"
  kill "${SERVER_PID:-}" "${MOUNT_PID:-}" &>/dev/null || true
  fusermount3 -u "${MOUNT_POINT}" &>/dev/null || true
  rm -rf "${CACHE_DIR}" "${MOUNT_POINT}"
}
trap cleanup EXIT

# prepare
rm -rf "${CACHE_DIR}" "${MOUNT_POINT}"
mkdir -p "${CACHE_DIR}" "${MOUNT_POINT}"

# 1) start HTTP backend
python3 backend/local_server.py \
  --port "${PORT}" \
  --directory "${PWD}/${REMOTE_DIR}" &
SERVER_PID=$!

# give it a moment
sleep 1

# 2) mount via FUSE against the data endpoint
"${REMOTE_CACHE}" \
  "${CACHE_DIR}" \
  "${URL}" \
  "${MOUNT_POINT}" \
  -s -f -d &
MOUNT_PID=$!

# give FUSE a moment to settle
sleep 1

echo "=== test_fuse ==="

# 3) sanity check: file_0.txt must exist
if [ ! -f "${MOUNT_POINT}/file_0.txt" ]; then
  echo "FAIL: file_0.txt not found in mount"
  exit 1
else
  echo "PASS: file_0.txt found in mount"
fi

# 4) verify that the contents match what's on the remote store
REMOTE_CONTENT=$(< "${REMOTE_DIR}/file_0.txt")
MOUNT_CONTENT=$(< "${MOUNT_POINT}/file_0.txt")

if [ "${MOUNT_CONTENT}" = "${REMOTE_CONTENT}" ]; then
  echo "PASS: file_0.txt content matches remote"
else
  echo "FAIL: file_0.txt content mismatch"
  exit 1
fi

# 5) write‐through smoke test
echo "smoke test" > "${MOUNT_POINT}/smoke.txt"
# let the write go through
sleep 0.5

if grep -q "smoke test" "${REMOTE_DIR}/smoke.txt"; then
  echo "PASS: write-through smoke.txt"
else
  echo "FAIL: smoke.txt was not written through to remote store"
  exit 1
fi

echo "All tests passed."

#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/debug-gtests-hang.sh [options] [--] [gtest args...]

Runs a native gtest binary, waits for a bounded interval, and if it is still
running attaches gdb to capture all thread backtraces before terminating it.

Options:
  --binary PATH   gtest executable to run (default: build/native/gtests-radio)
  --wait SECONDS  seconds to wait before attaching gdb (default: 30)
  --out-dir DIR   directory for logs/backtraces (default: build/debug)
  -h, --help      show this help

Examples:
  nix develop -c tools/debug-gtests-hang.sh -- --gtest_filter='PulsesTest.*'
  nix develop -c tools/debug-gtests-hang.sh --wait 10 -- --gtest_filter='Color*'
EOF
}

binary=${EDGE16_GTEST_BINARY:-build/native/gtests-radio}
wait_seconds=${EDGE16_GDB_WAIT_SECONDS:-30}
out_dir=${EDGE16_DEBUG_DIR:-build/debug}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary)
      binary=$2
      shift 2
      ;;
    --wait)
      wait_seconds=$2
      shift 2
      ;;
    --out-dir)
      out_dir=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

if [[ ! -x "$binary" ]]; then
  echo "error: gtest binary is not executable: $binary" >&2
  exit 2
fi

if ! [[ "$wait_seconds" =~ ^[0-9]+$ ]]; then
  echo "error: --wait must be a non-negative integer" >&2
  exit 2
fi

mkdir -p "$out_dir"
stamp=$(date -u +%Y%m%dT%H%M%SZ)
log="$out_dir/gtests-$stamp.log"
bt="$out_dir/gtests-$stamp.bt"

echo "running: $binary $*"
echo "log: $log"
echo "backtrace: $bt"

ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0:check_initialization_order=1}" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
  "$binary" "$@" >"$log" 2>&1 &
pid=$!

for _ in $(seq 1 "$wait_seconds"); do
  if ! kill -0 "$pid" 2>/dev/null; then
    set +e
    wait "$pid"
    status=$?
    set -e
    echo "gtests exited before gdb attach with status $status"
    tail -160 "$log" || true
    exit "$status"
  fi
  sleep 1
done

echo "gtests still running after ${wait_seconds}s; attaching gdb to pid $pid" | tee "$bt"
if command -v gdb >/dev/null 2>&1; then
  gdb -batch -p "$pid" \
    -ex 'set pagination off' \
    -ex 'info threads' \
    -ex 'thread apply all bt full' >>"$bt" 2>&1 || true
else
  echo "gdb not found in PATH; run through 'nix develop -c ...'" >>"$bt"
fi

kill -TERM "$pid" 2>/dev/null || true
sleep 1
kill -KILL "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true

echo "--- log tail ---"
tail -160 "$log" || true
echo "--- backtrace ---"
sed -n '1,260p' "$bt" || true
exit 124

#!/usr/bin/env bash
# G500 RC Car — RPi startup script
# Run with: npm run g500:start
#
# Starts all on-car services:
#   - camera/webcam_stream.py  → MJPEG stream on :8080
#   - server/index.js          → WebSocket + serial bridge + UI on :3000
#
# Ctrl+C (or any child exit) kills everything cleanly.

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ── Clean up any stale processes from a previous run ────────────────────────
echo "[g500] Cleaning up any previous instances..."
pkill -f "webcam_stream.py" 2>/dev/null && echo "[g500] Killed stale camera" || true
fuser -k 3000/tcp 2>/dev/null && echo "[g500] Killed stale server on :3000" || true
fuser -k 8080/tcp 2>/dev/null && echo "[g500] Killed stale camera on :8080" || true
sleep 0.5   # give ports time to release

# ── Kill all children when this script exits (Ctrl+C or child crash) ────────
trap 'echo ""; echo "[g500] Shutting down..."; kill 0' EXIT

echo ""
echo "[g500] Starting G500 RC Car services..."
echo "[g500] Repo: $REPO_DIR"
echo ""

# 1. Camera stream service
echo "[g500] Starting camera (webcam_stream.py → :8080)..."
python3 "$REPO_DIR/camera/webcam_stream.py" &
CAMERA_PID=$!

# 2. Node.js server (WebSocket + serial bridge + UI proxy)
echo "[g500] Starting server (index.js → :3000)..."
node "$REPO_DIR/server/index.js" &
SERVER_PID=$!

# Future: ROS2 launch goes here
# ros2 launch g500 g500_bringup.launch.py &

echo ""
echo "[g500] All services started."
echo "[g500] Camera stream: http://0.0.0.0:8080/stream"
echo "[g500] Server + UI:   http://0.0.0.0:3000"
echo "[g500] Press Ctrl+C to stop everything."
echo ""

# Exit (and trigger trap) if either child dies
wait $CAMERA_PID $SERVER_PID

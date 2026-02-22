#!/usr/bin/env bash
# G500 RC Car — RPi startup script
# Run with: npm run g500:start
#
# Starts all on-car services:
#   - camera/webcam_stream.py  → MJPEG stream on :8080
#   - server/index.js          → WebSocket + serial bridge + UI on :3000
#
# Future additions go here (ROS2 launch, bag recorder, etc.)
#
# Ctrl+C kills everything cleanly.

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Kill all child processes on exit (Ctrl+C or crash)
trap 'echo "[g500] Shutting down..."; kill 0' EXIT

echo "[g500] Starting G500 RC Car services..."
echo "[g500] Repo: $REPO_DIR"
echo ""

# 1. Camera stream service
echo "[g500] Starting camera (webcam_stream.py → :8080)..."
python3 "$REPO_DIR/camera/webcam_stream.py" &

# 2. Node.js server (WebSocket + serial bridge + UI proxy)
echo "[g500] Starting server (index.js → :3000)..."
node "$REPO_DIR/server/index.js" &

# Future: ROS2 launch goes here
# ros2 launch g500 g500_bringup.launch.py &

echo ""
echo "[g500] All services started."
echo "[g500] Camera stream: http://0.0.0.0:8080/stream"
echo "[g500] Server + UI:   http://0.0.0.0:3000"
echo "[g500] Press Ctrl+C to stop everything."
echo ""

# Wait for any child to exit (if one crashes, we exit and trap kills the rest)
wait

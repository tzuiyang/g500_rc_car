"""
G500 Webcam Stream Service
Captures frames from a standard USB webcam (V4L2 / UVC) via OpenCV
and serves them as an MJPEG stream over HTTP.

Endpoint: GET /stream   multipart/x-mixed-replace
Endpoint: GET /health   JSON health check

Usage:
  python3 camera/webcam_stream.py
  STREAM_PORT=8080 CAMERA_INDEX=0 python3 camera/webcam_stream.py
"""

import os
import time
import threading
import cv2
from flask import Flask, Response

HOST         = os.environ.get('STREAM_HOST',  '0.0.0.0')
PORT         = int(os.environ.get('STREAM_PORT',  '8080'))
CAMERA_INDEX = int(os.environ.get('CAMERA_INDEX', '0'))
JPEG_QUALITY = int(os.environ.get('JPEG_QUALITY', '80'))
TARGET_FPS   = int(os.environ.get('TARGET_FPS',   '30'))

app = Flask(__name__)

_lock  = threading.Lock()
_frame: bytes | None = None
_frame_count = 0


def capture_thread():
    global _frame, _frame_count

    cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cap.set(cv2.CAP_PROP_FPS, TARGET_FPS)

    if not cap.isOpened():
        print(f'[camera] ERROR: could not open /dev/video{CAMERA_INDEX}')
        return

    actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f'[camera] Opened /dev/video{CAMERA_INDEX} at {actual_w}x{actual_h}')

    encode_params = [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY]

    while True:
        ok, frame = cap.read()
        if not ok:
            print('[camera] Frame read failed — retrying in 1 s')
            time.sleep(1)
            cap.open(CAMERA_INDEX)
            continue

        _, jpeg = cv2.imencode('.jpg', frame, encode_params)
        with _lock:
            _frame = jpeg.tobytes()
            _frame_count += 1


def generate():
    """MJPEG frame generator for Flask streaming response."""
    frame_interval = 1.0 / TARGET_FPS
    while True:
        with _lock:
            frame = _frame
        if frame is None:
            time.sleep(0.05)
            continue
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
        )
        time.sleep(frame_interval)


@app.route('/stream')
def stream():
    return Response(
        generate(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )


@app.route('/health')
def health():
    with _lock:
        ready = _frame is not None
        count = _frame_count
    return {'status': 'ok', 'frame_ready': ready, 'frames_captured': count}


if __name__ == '__main__':
    t = threading.Thread(target=capture_thread, daemon=True)
    t.start()
    print(f'[camera] MJPEG stream → http://{HOST}:{PORT}/stream')
    print(f'[camera] Health check → http://{HOST}:{PORT}/health')
    app.run(host=HOST, port=PORT, threaded=True, use_reloader=False)

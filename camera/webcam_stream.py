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

# Disable OpenCV's OBSensor (Orbbec depth-camera) plugin before importing cv2.
# Without this it probes every V4L2 device as a depth camera, prints
# "Camera index out of range" errors, and can break the V4L2 capture.
os.environ.setdefault('OPENCV_VIDEOIO_PRIORITY_OBSENSOR', '0')

import cv2
from flask import Flask, Response

HOST         = os.environ.get('STREAM_HOST',  '0.0.0.0')
PORT         = int(os.environ.get('STREAM_PORT',  '8080'))
CAMERA_INDEX = int(os.environ.get('CAMERA_INDEX', '1'))
JPEG_QUALITY = int(os.environ.get('JPEG_QUALITY', '70'))
TARGET_FPS   = int(os.environ.get('TARGET_FPS',   '25'))
WIDTH        = int(os.environ.get('STREAM_WIDTH',  '640'))
HEIGHT       = int(os.environ.get('STREAM_HEIGHT', '480'))

app = Flask(__name__)

_lock       = threading.Lock()
_frame_event = threading.Event()   # signals a new frame is ready
_frame: bytes | None = None
_frame_count = 0


def _open_camera():
    """Open the webcam and return a configured VideoCapture."""
    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, TARGET_FPS)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)   # keep buffer minimal → low latency
    return cap


def capture_thread():
    global _frame, _frame_count

    encode_params = [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY]

    while True:
        cap = _open_camera()
        if not cap.isOpened():
            print(f'[camera] ERROR: could not open /dev/video{CAMERA_INDEX} — retrying in 2 s')
            cap.release()
            time.sleep(2)
            continue

        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        print(f'[camera] Opened /dev/video{CAMERA_INDEX} at {actual_w}x{actual_h} @ {TARGET_FPS}fps')

        failures = 0
        while True:
            ok, frame = cap.read()
            if not ok:
                failures += 1
                print(f'[camera] Frame read failed ({failures}) — retrying...')
                if failures >= 5:
                    print('[camera] Too many failures — releasing and reopening camera')
                    cap.release()
                    time.sleep(1)
                    break   # outer loop will recreate the capture object
                time.sleep(0.2)
                continue

            failures = 0
            _, jpeg = cv2.imencode('.jpg', frame, encode_params)
            with _lock:
                _frame = jpeg.tobytes()
                _frame_count += 1
            _frame_event.set()    # wake up any waiting generators


def generate():
    """MJPEG frame generator — event-driven, always sends the latest frame."""
    last_sent: bytes | None = None
    while True:
        _frame_event.wait(timeout=1.0)   # block until a new frame arrives
        _frame_event.clear()
        with _lock:
            frame = _frame
        if frame is None or frame is last_sent:
            continue
        last_sent = frame
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
        )


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

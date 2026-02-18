"""
G500 Camera Service
Captures RGB frames from OAK-D Lite via DepthAI and serves them
as an MJPEG stream over HTTP.

Endpoint: GET /stream  (multipart/x-mixed-replace)
"""

import os
import time
import threading
import cv2
import depthai as dai
from flask import Flask, Response

HOST = os.environ.get('STREAM_HOST', '0.0.0.0')
PORT = int(os.environ.get('STREAM_PORT', 8080))

app = Flask(__name__)

# Shared latest JPEG frame (protected by lock)
_lock = threading.Lock()
_frame: bytes | None = None


def camera_thread():
    """Runs DepthAI pipeline and updates _frame continuously."""
    global _frame

    pipeline = dai.Pipeline()

    # RGB camera node
    cam_rgb = pipeline.create(dai.node.ColorCamera)
    cam_rgb.setPreviewSize(640, 480)
    cam_rgb.setInterleaved(False)
    cam_rgb.setColorOrder(dai.ColorCameraProperties.ColorOrder.BGR)
    cam_rgb.setFps(30)

    # Output
    xout = pipeline.create(dai.node.XLinkOut)
    xout.setStreamName('rgb')
    cam_rgb.preview.link(xout.input)

    print(f'[camera] Starting OAK-D Lite pipeline…')

    with dai.Device(pipeline) as device:
        q = device.getOutputQueue(name='rgb', maxSize=4, blocking=False)
        print('[camera] Pipeline running.')

        while True:
            in_frame = q.get()
            frame = in_frame.getCvFrame()
            _, jpeg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 75])
            with _lock:
                _frame = jpeg.tobytes()


def generate():
    """Generator that yields MJPEG frames."""
    while True:
        with _lock:
            frame = _frame
        if frame is None:
            time.sleep(0.033)
            continue
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
        )
        time.sleep(0.033)  # ~30 fps cap


@app.route('/stream')
def stream():
    return Response(
        generate(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )


@app.route('/health')
def health():
    return {'status': 'ok', 'frame_ready': _frame is not None}


if __name__ == '__main__':
    t = threading.Thread(target=camera_thread, daemon=True)
    t.start()
    print(f'[camera] HTTP server on http://{HOST}:{PORT}/stream')
    app.run(host=HOST, port=PORT, threaded=True)

const { app, BrowserWindow } = require('electron')
const { spawn }              = require('child_process')
const path                   = require('path')
const http                   = require('http')

const CAMERA_PORT = 8080
const CAMERA_HOST = '127.0.0.1'

let cameraProcess = null
let mainWindow    = null

// ---------------------------------------------------------------------------
// Camera server
// ---------------------------------------------------------------------------

function startCameraServer() {
  const script = path.join(__dirname, '..', 'camera', 'webcam_stream.py')
  cameraProcess = spawn('python3', [script], {
    env: { ...process.env, STREAM_PORT: String(CAMERA_PORT), STREAM_HOST: CAMERA_HOST },
  })
  cameraProcess.stdout.on('data', d => process.stdout.write('[camera] ' + d))
  cameraProcess.stderr.on('data', d => process.stderr.write('[camera] ' + d))
  cameraProcess.on('exit', code => console.log(`[camera] exited (${code})`))
}

function waitForCamera(retries = 40, delayMs = 500) {
  return new Promise((resolve, reject) => {
    const attempt = (n) => {
      const req = http.get(
        { hostname: CAMERA_HOST, port: CAMERA_PORT, path: '/health', timeout: 1000 },
        (res) => { res.resume(); resolve() }
      )
      req.on('error', () => {
        if (n <= 0) return reject(new Error('Camera server did not start in time'))
        setTimeout(() => attempt(n - 1), delayMs)
      })
      req.on('timeout', () => { req.destroy(); setTimeout(() => attempt(n - 1), delayMs) })
    }
    attempt(retries)
  })
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

function createWindow(cameraReady) {
  mainWindow = new BrowserWindow({
    width:           1280,
    height:          800,
    minWidth:        800,
    minHeight:       500,
    backgroundColor: '#0a0a0a',
    title:           'G500 FPV',
    webPreferences: {
      preload:          path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration:  false,
    },
  })

  mainWindow.loadFile(path.join(__dirname, 'index.html'))

  // Pass camera status to renderer once DOM is ready
  mainWindow.webContents.on('did-finish-load', () => {
    mainWindow.webContents.send('camera-status', {
      ready:  cameraReady,
      url:    `http://${CAMERA_HOST}:${CAMERA_PORT}/stream`,
      health: `http://${CAMERA_HOST}:${CAMERA_PORT}/health`,
    })
  })
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

app.whenReady().then(async () => {
  startCameraServer()

  let cameraReady = false
  try {
    await waitForCamera()
    cameraReady = true
    console.log('[app] Camera server ready')
  } catch (err) {
    console.error('[app] Camera server not ready:', err.message)
  }

  createWindow(cameraReady)
})

app.on('before-quit', () => {
  if (cameraProcess) {
    cameraProcess.kill()
    cameraProcess = null
  }
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})

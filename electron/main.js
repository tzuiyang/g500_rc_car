/**
 * G500 Electron — dumb display shell.
 *
 * This file has no business logic. Its only job:
 *   1. Read G500_URL (or g500.config.json) to know where the RPi server is
 *   2. Open a window
 *   3. Tell the renderer what URLs to connect to
 *
 * Everything else (camera, serial, ROS) runs on the RPi.
 *
 * Laptop usage:
 *   npm run app                              ← uses rpiUrl from g500.config.json
 *   G500_URL=http://192.168.1.107:3000 npm run app   ← env var override
 *
 * To change the RPi IP, edit g500.config.json at the repo root.
 */

const { app, BrowserWindow } = require('electron')
const path = require('path')

const config = require('../g500.config.json')
const BASE = (process.env.G500_URL || config.rpiUrl).replace(/\/$/, '')

function createWindow() {
  const win = new BrowserWindow({
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

  win.loadFile(path.join(__dirname, 'index.html'))

  win.webContents.on('did-finish-load', () => {
    win.webContents.send('camera-status', {
      ready:     true,
      streamUrl: `${BASE}/stream`,
      wsUrl:     BASE.replace(/^http/, 'ws'),
    })
  })
}

app.whenReady().then(() => {
  console.log(`[app] G500 server: ${BASE}`)
  createWindow()
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})

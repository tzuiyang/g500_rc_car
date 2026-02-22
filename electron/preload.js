const { contextBridge, ipcRenderer } = require('electron')

// Expose only what the renderer needs — no Node/Electron APIs leaked.
contextBridge.exposeInMainWorld('g500', {
  // Fires once on load with { ready, streamUrl, wsUrl }
  onReady: (cb) => ipcRenderer.on('camera-status', (_e, data) => cb(data)),
})

const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('g500', {
  onCameraStatus: (cb) => ipcRenderer.on('camera-status', (_e, data) => cb(data)),
})

const { app, BrowserWindow, ipcMain } = require("electron");
const path = require("path");

const bench = process.argv.includes("bench");
if (process.platform === "linux") {
  app.commandLine.appendSwitch("no-sandbox");
  app.commandLine.appendSwitch("disable-gpu");
}
app.commandLine.appendSwitch("disable-frame-rate-limit");
app.commandLine.appendSwitch("disable-gpu-vsync");
app.commandLine.appendSwitch("disable-renderer-backgrounding");
app.commandLine.appendSwitch("disable-background-timer-throttling");

ipcMain.handle("mode", () => (bench ? "bench" : "hello"));
ipcMain.handle("ready", () => {
  process.stdout.write("READY\n");
});
ipcMain.handle("report", (e, l) => {
  if (l === "DONE") {
    app.exit(0);
    return;
  }
  process.stdout.write(l + "\n");
});

app.whenReady().then(() => {
  const w = new BrowserWindow({
    width: 960,
    height: 640,
    title: "Hello",
    webPreferences: { nodeIntegration: true, contextIsolation: false, backgroundThrottling: false },
  });
  w.loadFile(path.join(__dirname, "index.html"));
});

app.on("window-all-closed", () => app.quit());

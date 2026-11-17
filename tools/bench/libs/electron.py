import json, os, shutil, sys, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_shared import dir_bytes


def run(ctx):
    src = ctx.bench / "electron"
    d = ctx.work / "electron"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    for f in ("main.js", "package.json"):
        shutil.copy(src / f, d / f)
    shutil.copy(ctx.bench / "webpage" / "index.html", d / "index.html")
    npm = "npm.cmd" if ctx.plat == "windows" else "npm"
    t = time.perf_counter()
    ctx.sh([npm, "install", "electron@latest", "--no-audit", "--no-fund"], cwd=d)
    out = {"npm_install_s": round(time.perf_counter() - t, 1)}
    ctx.sh(["node", "node_modules/electron/install.js"], cwd=d)
    pkg = json.loads((d / "node_modules/electron/package.json").read_text())
    out["version"] = pkg["version"]
    out["commit"] = "v" + pkg["version"]
    dist = d / "node_modules/electron/dist"
    out["install_bytes_real"] = dir_bytes(dist)
    out["real_includes"] = "node_modules/electron/dist (Chromium + Node runtime, locales, ffmpeg, resources), unstripped as shipped"
    if ctx.plat == "windows":
        exe = dist / "electron.exe"
    elif ctx.plat == "macos":
        exe = dist / "Electron.app/Contents/MacOS/Electron"
    else:
        exe = dist / "electron"
    out["electron_binary_bytes"] = exe.stat().st_size
    out["stripped_bytes_hello_null"] = {"na": "Electron has no headless or null backend, every app is the full Chromium runtime, see install_bytes_real"}
    out["hello_includes"] = "n/a, hello is index.html (about 3 KB) + main.js loaded by the stock electron binary"
    out["compile_s_hello"] = {"na": "no compile step, the app is interpreted JS and HTML"}
    cmd = [str(exe), str(d)]
    if ctx.plat == "linux":
        cmd.insert(1, "--no-sandbox")
    out["frame_ms_dom"] = ctx.frames(cmd + ["bench"], timeout=900)
    out["frame_includes"] = "retained DOM: hello and dash scenes mutate existing nodes (50 rows of span, button, checkbox, range input) then force style+layout via offsetHeight, no paint, measured with performance.now; the *_raf scenes do the same mutation then await requestAnimationFrame and include paint and composite (software compositing on Linux, frame limit and vsync disabled via switches, may still be display capped, read the numbers accordingly); 100 warmup + 2000 frames"
    try:
        out["first_frame_real"] = ctx.first_frame_n(cmd, n=5, settle=2.0, timeout=90)
        out["first_frame_includes"] = "process start to the renderer having painted twice via requestAnimationFrame and reporting over IPC, rss_mb sums the main process and all child processes"
    except Exception as e:
        out["first_frame_real"] = {"error": str(e)[-800:]}
    out["bench_src_lines"] = ctx.loc([src / "main.js", ctx.bench / "webpage" / "index.html"])
    out["notes"] = ["Linux runs with --no-sandbox and --disable-gpu (xvfb, no GPU)", "window size 960x640 for both scenes, nodeIntegration on to reach ipcRenderer", "dash rows are inline-flex style rows, the 50 rows overflow 640 px and scroll, all are laid out"]
    return out

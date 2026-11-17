import os, re, shutil, sys, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_shared import ldd_sizes, summarize


def run(ctx):
    src = ctx.bench / "tauri"
    d = ctx.work / "tauri"
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(src, d)
    web = ctx.work / "webpage"
    shutil.rmtree(web, ignore_errors=True)
    web.mkdir(parents=True)
    shutil.copy(ctx.bench / "webpage" / "index.html", web / "index.html")
    env = {"CARGO_TARGET_DIR": str(d / "target")}
    t = time.perf_counter()
    ctx.sh(["cargo", "build", "--release"], cwd=d, env=env, timeout=3000)
    out = {"compile_s_hello": round(time.perf_counter() - t, 1), "compile_runs": 1, "compile_includes": "full cold cargo build --release including tauri, wry, tao and all dependencies, single run"}
    lock = (d / "Cargo.lock").read_text()
    m = re.search(r'name = "tauri"\nversion = "([^"]+)"', lock)
    out["version"] = m.group(1) if m else None
    out["commit"] = "tauri " + str(out["version"]) + " from crates.io"
    exe = d / "target" / "release" / ("bench-tauri" + ctx.exe)
    out["stripped_bytes_hello_real"] = ctx.stripped_size(exe)
    out["real_includes"] = "release binary (opt-level=s, lto, strip, panic=abort) with the html embedded, system webview NOT included (WebKitGTK / WebView2 / WKWebView come from the OS)"
    out["stripped_bytes_hello_null"] = {"na": "Tauri has no headless backend, the only binary is the one with the system webview linked or loaded, see stripped_bytes_hello_real"}
    out["hello_includes"] = "see real_includes"
    if ctx.plat == "linux":
        out.update(summarize(ldd_sizes(ctx, exe), r"webkit|javascriptcore|gtk|soup"))
        out["shared_libs_matching"] = "webkit, javascriptcore, gtk, soup"
        env.update({"WEBKIT_DISABLE_COMPOSITING_MODE": "1", "WEBKIT_DISABLE_DMABUF_RENDERER": "1", "GDK_BACKEND": "x11"})
    cmd = [str(exe)]
    out["frame_ms_dom"] = ctx.frames(cmd + ["bench"], env=env, timeout=900)
    out["frame_includes"] = "retained DOM: hello and dash scenes mutate existing nodes (50 rows of span, button, checkbox, range input) then force style+layout via offsetHeight, no paint, measured with performance.now; the *_raf scenes do the same mutation then await requestAnimationFrame and include paint and composite in the system webview (frame rate may be display capped at 60 Hz, read the numbers accordingly); 100 warmup + 2000 frames"
    try:
        out["first_frame_real"] = ctx.first_frame_n(cmd, n=5, env=env, settle=2.0, timeout=90)
        out["first_frame_includes"] = "process start to the page having painted twice via requestAnimationFrame and reporting through invoke, rss_mb sums the app and all webview child processes (WebView2 msedgewebview2 children are only counted when spawned under the app process tree)"
    except Exception as e:
        out["first_frame_real"] = {"error": str(e)[-800:]}
    out["bench_src_lines"] = ctx.loc([src / "src/main.rs", ctx.bench / "webpage" / "index.html"])
    out["notes"] = ["window size 960x640, same html page as the electron lib", "no cargo-tauri CLI, plain tauri crate with generate_context and an embedded frontendDist", "Linux sets WEBKIT_DISABLE_COMPOSITING_MODE and WEBKIT_DISABLE_DMABUF_RENDERER for xvfb"]
    return out

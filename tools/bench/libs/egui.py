import os
import sys
import time
import shutil
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "rbench"))

TAG = "0.37.0"


def run(ctx):
    d, sha = ctx.clone("https://github.com/emilk/egui.git", TAG)
    proj = ctx.bench / "egui"
    target = ctx.work / "egui_target"
    manifest = str(proj / "Cargo.toml")
    cargo = ["cargo"]
    out = {"version": TAG, "commit": sha}
    rs = []
    for c in ("egui", "epaint", "emath", "ecolor"):
        rs += [str(p) for p in (d / "crates" / c / "src").rglob("*.rs")]
    out["core_lines"] = ctx.loc(rs)
    out["core_lines_note"] = "rust lines in crates egui, epaint, emath, ecolor src"
    out["rustc"] = ctx.version(["rustc", "--version"])

    ctx.sh(cargo + ["fetch", "--manifest-path", manifest], timeout=900)
    exe = target / "release" / ("hello_null" + ctx.exe)
    best = None
    for _ in range(3):
        if target.exists():
            shutil.rmtree(target, ignore_errors=True)
        t = time.perf_counter()
        ctx.sh(cargo + ["build", "--release", "--manifest-path", manifest, "--target-dir", str(target), "--bin", "hello_null"], timeout=1800)
        t = time.perf_counter() - t
        best = t if best is None or t < best else best
    out["compile_s_hello"] = round(best, 3)
    out["compile_s_hello_note"] = "clean cargo build --release of hello_null including egui and all dependencies (registry sources already fetched), best of 3"
    out["hello_null_output"] = ctx.sh([str(exe)]).stdout.strip()
    out["stripped_bytes_hello_null"] = ctx.stripped_size(exe)
    out["hello_includes"] = "egui + epaint + emath, default fonts and font atlas build, Context::run_ui + tessellate, no renderer and no window; opt-level=s, lto, strip"

    ctx.sh(cargo + ["build", "--release", "--manifest-path", manifest, "--target-dir", str(target), "--bin", "bench"], timeout=1800)
    out["frame_ms"] = ctx.frames([str(target / "release" / ("bench" + ctx.exe)), "2000"])
    out["frame_includes"] = "ui build + layout + tessellation into triangle meshes (Context::run_ui then tessellate, 960x640, 1.0 ppp), no rasterization or GPU upload; immediate mode, all 50 rows built every frame, rows beyond 640px still laid out and tessellated"

    try:
        ctx.sh(cargo + ["build", "--release", "--manifest-path", manifest, "--target-dir", str(target), "--bin", "hello_real", "--features", "real"], timeout=2400)
        real = target / "release" / ("hello_real" + ctx.exe)
        out["stripped_bytes_hello_real"] = ctx.stripped_size(real)
        out["real_includes"] = "eframe 0.37 with glow backend (winit + glutin + egui_glow), static, GL libs dynamic and not counted; opt-level=s, lto, strip"
        if ctx.plat == "windows":
            import mesa_win
            out["mesa"] = mesa_win.install(ctx, real.parent)
        if ctx.plat == "linux" and not os.environ.get("DISPLAY"):
            out["first_frame_real"] = {"error": "no DISPLAY"}
        else:
            out["first_frame_real"] = ctx.first_frame_n([str(real)], n=5, timeout=90)
    except Exception as e:
        out["first_frame_real"] = {"error": str(e)[-1500:]}
    out["notes"] = [
        "egui frame cost excludes rasterization: it stops at tessellated meshes, a GPU or software rasterizer would add to it",
        "headless frame loop uses egui::Context::run_ui with a 960x640 RawInput and no window",
        "slider is 0.0..=1.0 with a drag value box",
        "real window runs under Mesa llvmpipe on Linux and Windows (mesa-dist-win), native OpenGL on macOS",
    ]
    return out

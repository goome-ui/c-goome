import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TAG = "v9.6.0"


def sources(d):
    return sorted(str(p) for p in (d / "src").rglob("*.c"))


def build(ctx, d, out, srcs, defs, extra_inc, jobs):
    if out.exists():
        import shutil
        shutil.rmtree(out)
    out.mkdir(parents=True)
    inc = ["-I" + str(d), "-I" + str(ctx.bench / "lvgl"), "-DLV_CONF_INCLUDE_SIMPLE"] + extra_inc
    base = [ctx.cc, "-std=gnu11", "-Os", "-ffunction-sections", "-fdata-sections", "-w", "-c"] + inc + defs
    objs = []
    jobs_list = []
    for i, s in enumerate(srcs):
        o = str(out / ("%d.o" % i))
        objs.append(o)
        jobs_list.append(base + ["-o", o, s])
    with ThreadPoolExecutor(jobs) as ex:
        list(ex.map(lambda c: ctx.sh(c), jobs_list))
    return objs


def link(ctx, out, objs, mains, exe, libs, defs, extra_inc, d):
    rsp = out / "objs.rsp"
    rsp.write_text("\n".join(o.replace("\\", "/") for o in objs))
    gc = "-Wl,-dead_strip" if ctx.plat == "macos" else "-Wl,--gc-sections"
    inc = ["-I" + str(d), "-I" + str(ctx.bench / "lvgl"), "-DLV_CONF_INCLUDE_SIMPLE"] + extra_inc
    cmd = [ctx.cc, "-std=gnu11", "-Os", "-ffunction-sections", "-fdata-sections", "-w"] + inc + defs + ["-o", str(exe)] + mains + ["@" + str(rsp), gc] + libs
    ctx.sh(cmd)


def run(ctx):
    d, sha = ctx.clone("https://github.com/lvgl/lvgl.git", TAG)
    jobs = int(os.environ.get("BENCH_JOBS") or os.cpu_count() or 2)
    srcs = sources(d)
    mathl = [] if ctx.plat == "windows" else ["-lm"]
    out = {"version": TAG, "commit": sha, "source_files_compiled": len(srcs), "compile_jobs": jobs}

    core = []
    for sub in ("core", "display", "tick", "misc", "stdlib", "layouts"):
        core += [str(p) for p in (d / "src" / sub).rglob("*") if p.suffix in (".c", ".h")]
    out["core_lines"] = ctx.loc(core)
    out["all_src_lines"] = ctx.loc([str(p) for p in (d / "src").rglob("*") if p.suffix in (".c", ".h")])

    work = ctx.work / "lvgl_build"
    hello = work / ("hello_null" + ctx.exe)
    best = None
    import time
    for _ in range(3):
        t = time.perf_counter()
        objs = build(ctx, d, work / "o", srcs, [], [], jobs)
        link(ctx, work / "o", objs, [str(ctx.bench / "lvgl/hello_null.c")], hello, mathl, [], [], d)
        t = time.perf_counter() - t
        best = t if best is None or t < best else best
    out["compile_s_hello"] = round(best, 3)
    out["compile_s_hello_note"] = "full library from source (%d TUs, -Os) compiled with %d parallel jobs plus link, best of 3" % (len(srcs), jobs)
    r = ctx.sh([str(hello)])
    out["hello_null_output"] = r.stdout.strip()
    out["stripped_bytes_hello_null"] = ctx.stripped_size(hello)
    out["hello_includes"] = "LVGL core + widgets + default theme + Montserrat 14 + software renderer, RAM framebuffer display with no-op flush, no window system; linked with --gc-sections, clib stdlib"

    bench = work / ("bench" + ctx.exe)
    link(ctx, work / "o", objs, [str(ctx.bench / "lvgl/bench.c")], bench, mathl, [], [], d)
    out["frame_ms"] = ctx.frames([str(bench), "2000"])
    out["frame_includes"] = "full software render (lv_timer_handler with the screen invalidated every frame, 960x640 XRGB8888 RAM framebuffer, flush callback is a no-op); retained-mode widgets built once, so no per-frame widget submission; rows past 640px are clipped by the renderer"

    sdl = work / "sdl"
    flags = None
    if ctx.plat == "windows":
        flags = sdl_windows(ctx, work)
    else:
        sc = ctx.sh(["sdl2-config", "--cflags"], check=False)
        sl = ctx.sh(["sdl2-config", "--libs"], check=False)
        if sc.returncode == 0 and sl.returncode == 0:
            flags = (sc.stdout.split(), sl.stdout.split(), None)
    if flags is None:
        out["real"] = {"na": "SDL2 development files not found (sdl2-config missing)"}
        out["notes"] = notes()
        return out
    cflags, lflags, dll = flags
    cflags = cflags + ["-DBENCH_SDL"]
    try:
        objs2 = build(ctx, d, sdl, srcs, ["-DBENCH_SDL"], cflags, jobs)
        real = work / ("hello_real" + ctx.exe)
        link(ctx, sdl, objs2, [str(ctx.bench / "lvgl/hello_real.c")], real, lflags + mathl, ["-DBENCH_SDL"], cflags, d)
        out["stripped_bytes_hello_real"] = ctx.stripped_size(real)
        out["real_includes"] = "LVGL + its lv_sdl_window driver (SDL2 software renderer), SDL2 linked dynamically and not counted"
        env = {}
        if dll:
            env["PATH"] = dll + os.pathsep + os.environ.get("PATH", "")
        if ctx.plat == "linux" and not os.environ.get("DISPLAY"):
            out["first_frame_real"] = {"na": "no DISPLAY"}
        else:
            out["first_frame_real"] = ctx.first_frame_n([str(real)], n=5, env=env)
    except Exception as e:
        out["first_frame_real"] = {"error": str(e)[-1200:]}
    out["notes"] = notes()
    return out


def sdl_windows(ctx, work):
    import urllib.request
    import tarfile
    url = "https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz"
    tgz = work / "sdl.tar.gz"
    urllib.request.urlretrieve(url, tgz)
    with tarfile.open(tgz) as t:
        t.extractall(work / "sdlpkg")
    root = work / "sdlpkg" / "SDL2-2.32.10" / "x86_64-w64-mingw32"
    return (["-I" + str(root / "include" / "SDL2")], ["-L" + str(root / "lib"), "-lSDL2"], str(root / "bin"))


def notes():
    return [
        "LVGL is retained-mode: the 50 rows are created once, per frame cost is invalidate + layout check + full software redraw of the invalidated screen",
        "slider range is integer 0..100 (LVGL sliders are int based)",
        "LV_USE_OS none, LV_USE_STDLIB clib, LV_COLOR_FORMAT_DEFAULT XRGB8888, default theme, Montserrat 14 font",
        "dash rows are flex rows in a scrollable screen, only the first rows fit inside 640px, the rest are clipped",
        "real window uses LVGL's SDL driver with SDL's software renderer",
    ]

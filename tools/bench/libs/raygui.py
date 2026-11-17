import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "rbench"))

RAYLIB = "6.2"
RAYGUI = "5.0"


def cmake_build(ctx, src, bdir, platform, extra, jobs):
    import shutil
    if bdir.exists():
        shutil.rmtree(bdir, ignore_errors=True)
    cfg = ["cmake", "-S", str(src), "-B", str(bdir), "-DCMAKE_BUILD_TYPE=MinSizeRel", "-DBUILD_EXAMPLES=OFF", "-DPLATFORM=" + platform, "-DCUSTOMIZE_BUILD=ON", "-DUSE_AUDIO=OFF", "-DCMAKE_C_COMPILER=" + ctx.cc]
    if ctx.plat == "windows":
        cfg += ["-G", "MinGW Makefiles"]
    ctx.sh(cfg + extra)
    ctx.sh(["cmake", "--build", str(bdir), "-j", str(jobs)])
    libs = list(bdir.rglob("libraylib.a"))
    if not libs:
        raise RuntimeError("libraylib.a not found in " + str(bdir))
    return libs[0]


def sys_libs(ctx, kind):
    if ctx.plat == "linux":
        return ["-lGL", "-lm", "-lpthread", "-ldl", "-lrt", "-lX11"] if kind == "glfw" else ["-lm", "-lpthread"]
    if ctx.plat == "macos":
        return ["-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "OpenGL", "-framework", "QuartzCore", "-framework", "Metal", "-lm"] if kind == "glfw" else ["-lm"]
    return ["-lopengl32", "-lgdi32", "-lwinmm"] if kind in ("glfw", "win32") else ["-lwinmm"]


def compile_prog(ctx, lib, rsrc, rguisrc, srcfile, exe, kind, defs=()):
    gc = "-Wl,-dead_strip" if ctx.plat == "macos" else "-Wl,--gc-sections"
    cmd = [ctx.cc, "-std=gnu99", "-Os", "-ffunction-sections", "-fdata-sections", "-w", "-I" + str(rsrc), "-I" + str(rguisrc)] + list(defs) + ["-o", str(exe), srcfile, str(lib), gc] + sys_libs(ctx, kind)
    ctx.sh(cmd)


def run(ctx):
    rl, rlsha = ctx.clone("https://github.com/raysan5/raylib.git", RAYLIB)
    rg, rgsha = ctx.clone("https://github.com/raysan5/raygui.git", RAYGUI)
    jobs = int(os.environ.get("BENCH_JOBS") or os.cpu_count() or 2)
    prog = ctx.bench / "raygui"
    work = ctx.work / "raygui_build"
    work.mkdir(exist_ok=True)
    rsrc = rl / "src"
    rguisrc = rg / "src"
    out = {"version": "raygui %s + raylib %s" % (RAYGUI, RAYLIB), "commit": "raygui %s raylib %s" % (rgsha, rlsha), "raylib_audio_module": "disabled (USE_AUDIO=OFF), raylib built MinSizeRel -Os, GLFW vendored"}
    out["core_lines"] = ctx.loc([str(rguisrc / "raygui.h")])
    out["core_lines_note"] = "raygui.h only; raylib itself is " + str(ctx.loc([str(p) for p in rsrc.glob("*.[ch]")] + [str(p) for p in (rsrc / "platforms").glob("*.c")])) + " lines (src/*.c,h + platforms)"

    mem = None
    try:
        mlib = cmake_build(ctx, rl, work / "b_mem", "Memory", [], jobs)
        hn = work / ("hello_null" + ctx.exe)
        compile_prog(ctx, mlib, rsrc, rguisrc, str(prog / "hello.c"), hn, "mem", ["-DHELLO_ONESHOT"])
        r = ctx.sh([str(hn)], check=False)
        out["hello_null_exit"] = r.returncode
        if r.returncode:
            raise RuntimeError("hello_null exit %d: %s %s" % (r.returncode, r.stdout[-500:], r.stderr[-500:]))
        out["stripped_bytes_hello_null"] = ctx.stripped_size(hn)
        out["hello_includes"] = "raygui + raylib core built with PLATFORM_MEMORY (rlsw software rasterizer, memory framebuffer, no window system, no GPU), built-in font"
        bm = work / ("bench_mem" + ctx.exe)
        compile_prog(ctx, mlib, rsrc, rguisrc, str(prog / "bench.c"), bm, "mem")
        out["frame_ms_memory"] = ctx.frames([str(bm), "2000"])
        out["frame_memory_includes"] = "raylib PLATFORM_MEMORY: full rlsw software rasterization of 960x640 plus framebuffer copy to RAM per frame"
    except Exception as e:
        out["memory_platform"] = {"error": str(e)[-1500:]}

    configs = [("glfw", "Desktop", [])]
    if ctx.plat == "windows":
        configs.append(("win32", "Win32", ["-DOPENGL_VERSION=Software"]))
    tried = {}
    real_ok = False
    for kind, platform, extra in configs:
        try:
            best = None
            for _ in range(3):
                t = time.perf_counter()
                lib = cmake_build(ctx, rl, work / "b_real", platform, extra, jobs)
                hr = work / ("hello_real" + ctx.exe)
                compile_prog(ctx, lib, rsrc, rguisrc, str(prog / "hello.c"), hr, kind)
                t = time.perf_counter() - t
                best = t if best is None or t < best else best
            out["compile_s_hello"] = round(best, 3)
            out["compile_s_hello_note"] = "raylib %s from source via cmake (%s, -j%d) + hello with raygui implementation, best of 3" % (RAYLIB, platform, jobs)
            out["stripped_bytes_hello_real"] = ctx.stripped_size(hr)
            if ctx.plat == "linux":
                out["real_includes"] = "raygui + raylib static + vendored GLFW; libGL and libX11 dynamic and not counted"
            elif ctx.plat == "macos":
                out["real_includes"] = "raygui + raylib static + vendored GLFW; system frameworks not counted"
            elif kind == "glfw":
                out["real_includes"] = "raygui + raylib static + vendored GLFW; opengl32/gdi32 dynamic and not counted"
            else:
                out["real_includes"] = "raygui + raylib static with native Win32 platform and rlsw software renderer (GDI present)"
            bn = work / ("bench" + ctx.exe)
            compile_prog(ctx, lib, rsrc, rguisrc, str(prog / "bench.c"), bn, kind)
            if ctx.plat == "windows" and kind == "glfw":
                import mesa_win
                out["mesa"] = mesa_win.install(ctx, work)
            out["real_backend"] = platform
            fi = None
            if ctx.plat == "linux" and not os.environ.get("DISPLAY"):
                raise RuntimeError("no DISPLAY")
            fi = ctx.first_frame_n([str(hr)], n=5)
            out["first_frame_real"] = fi
            out["frame_ms"] = ctx.frames([str(bn), "2000"])
            real_ok = True
            break
        except Exception as e:
            tried[platform] = str(e)[-1200:]
    if tried:
        out["real_attempt_errors"] = tried
    if not real_ok:
        out["first_frame_real"] = {"error": "; ".join("%s: %s" % kv for kv in tried.items())[-1500:]}
        out["notes"] = notes(ctx, None)
        return out
    if ctx.plat == "linux":
        out["frame_includes"] = "full GL rasterization on llvmpipe/software GL (Mesa llvmpipe under xvfb), 960x640 window, BeginDrawing/EndDrawing loop with SetTargetFPS(0) and swap interval 0, includes raygui immediate-mode build, rlgl batch flush and buffer swap"
    elif ctx.plat == "macos":
        out["frame_includes"] = "full GL rasterization on the runner's software/virtualized macOS OpenGL (no real GPU), 960x640 window, BeginDrawing/EndDrawing loop with SetTargetFPS(0), swap interval 0, includes raygui build and buffer swap"
    elif out["real_backend"] == "Desktop":
        out["frame_includes"] = "full GL rasterization on Mesa llvmpipe for Windows (mesa-dist-win opengl32.dll), 960x640 window, BeginDrawing/EndDrawing loop with SetTargetFPS(0), swap interval 0, includes raygui build and buffer swap"
    else:
        out["frame_includes"] = "raylib rlsw software rasterizer through the native Win32 platform (GRAPHICS_API_OPENGL_SOFTWARE, GDI present), 960x640 window, BeginDrawing/EndDrawing loop with SetTargetFPS(0); GLFW + Mesa path failed, see real_attempt_errors"
    out["notes"] = notes(ctx, out)
    return out


def notes(ctx, out):
    return [
        "raygui is immediate mode on raylib: all 50 rows are submitted every frame, rows past 640px are drawn but clipped by the rasterizer",
        "default raygui dark-free light style and raylib built-in bitmap font",
        "raylib audio module disabled in the build",
        "frame_ms measures a real window loop, rasterization cost depends on the runner's software GL",
    ]

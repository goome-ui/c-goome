import os, re, shutil, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_shared import ldd_sizes, summarize, dir_bytes


def run(ctx):
    src = ctx.bench / "gtk4"
    d = ctx.work / "gtk4"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    env = {"GSK_RENDERER": "cairo"}
    cc = ctx.cc
    pk = "pkg-config"
    if ctx.plat == "windows":
        root = Path(os.environ.get("MSYS2_ROOT", r"C:\msys64")) / "mingw64"
        cc = str(root / "bin" / "gcc.exe")
        pk = str(root / "bin" / "pkgconf.exe")
        if not os.path.exists(pk):
            pk = str(root / "bin" / "pkg-config.exe")
        env["PATH"] = str(root / "bin") + os.pathsep + os.environ["PATH"]
    else:
        env["GDK_BACKEND"] = "x11" if ctx.plat == "linux" else os.environ.get("GDK_BACKEND", "")
        if not env["GDK_BACKEND"]:
            del env["GDK_BACKEND"]
    flags = ctx.sh([pk, "--cflags", "--libs", "gtk4"], env=env).stdout.split()
    out = {"version": ctx.sh([pk, "--modversion", "gtk4"], env=env).stdout.strip()}
    out["commit"] = out["version"]
    hello = d / ("hello" + ctx.exe)
    cmd = [cc, "-Os", "-o", str(hello), str(src / "hello.c")] + flags
    if ctx.plat == "macos":
        cmd += ["-Wl,-dead_strip"]
    else:
        cmd += ["-Wl,--gc-sections"]
    out["compile_s_hello"] = ctx.timed(cmd, env=env, runs=3)
    out["stripped_bytes_hello_null"] = ctx.stripped_size(hello)
    out["hello_includes"] = "gtk_application_window with label/button/label, stripped binary only (GTK has no null backend, it needs a display at run time), shared GTK not included"
    out["stripped_bytes_hello_real"] = out["stripped_bytes_hello_null"]
    out["real_includes"] = "same binary, see shared_libs_* for the libraries needed"
    if ctx.plat == "windows":
        dll = {}
        for ln in ctx.sh(["objdump", "-p", str(hello)], env=env, check=False).stdout.splitlines():
            m = re.search(r"DLL Name: (\S+)", ln)
            if m:
                dll[m.group(1)] = 1
        seen = {}
        todo = list(dll)
        while todo:
            n = todo.pop()
            p = Path(root) / "bin" / n
            if n in seen or not p.exists():
                continue
            seen[n] = p.stat().st_size
            for ln in ctx.sh(["objdump", "-p", str(p)], env=env, check=False).stdout.splitlines():
                m = re.search(r"DLL Name: (\S+)", ln)
                if m:
                    todo.append(m.group(1))
        out["shared_libs_count"] = len(seen)
        out["shared_libs_bytes_all"] = sum(seen.values())
        out["shared_libs_bytes_matching"] = sum(v for k, v in seen.items() if re.search(r"gtk|gdk|gsk|glib|gobject|gio|pango|cairo|graphene|harfbuzz|epoxy|gdk_pixbuf", k))
    else:
        out.update(summarize(ldd_sizes(ctx, hello), r"gtk|gdk|glib|gobject|gio|pango|cairo|graphene|harfbuzz|epoxy|pixbuf"))
    out["shared_libs_matching"] = "gtk, gdk, glib, gobject, gio, pango, cairo, graphene, harfbuzz, epoxy, pixbuf libraries"
    if ctx.plat == "linux":
        out["ldd"] = [x.strip() for x in ctx.sh(["ldd", str(hello)], check=False).stdout.split("\n") if x.strip()]
    out["static_gtk"] = {"na": "GTK4 is not built or packaged statically on any of the three package managers"}

    bench = d / ("bench" + ctx.exe)
    ctx.sh([cc, "-Os", "-o", str(bench), str(src / "bench.c")] + flags, env=env)
    ok = ctx.plat != "linux" or os.environ.get("DISPLAY")
    if ok:
        try:
            out["frame_ms_cairo"] = ctx.frames([str(bench)], env=env)
        except Exception as e:
            out["frame_ms_cairo"] = {"error": str(e)[-800:]}
        try:
            out["first_frame_real"] = ctx.first_frame_n([str(hello)], n=5, env=env)
        except Exception as e:
            out["first_frame_real"] = {"error": str(e)[-800:]}
    else:
        out["frame_ms_cairo"] = {"na": "no DISPLAY"}
    out["frame_includes"] = "mapped window, per frame: widget updates + gtk_widget_measure + gtk_widget_allocate + widget snapshot to a GskRenderNode + gsk_render_node_draw into a cairo ARGB32 image (software cairo rasterization, no presentation, no frame clock), 100 warmup + 2000 frames per scene"
    out["bench_src_lines"] = ctx.loc([src / "hello.c", src / "bench.c"])
    out["notes"] = [
        "GSK_RENDERER=cairo for first_frame_real so no GPU is needed",
        "dash is 50 rows of label, button, check button, scale, a Value label, in a vertical box clipped to a 960x640 surface",
        "compile_s_hello is plain gcc -Os of hello.c against the GTK headers, best of 3",
    ]
    return out

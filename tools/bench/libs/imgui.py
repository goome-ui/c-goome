import json
import os
import shutil
import subprocess
import urllib.request
from pathlib import Path

import cb_common

TAG = "v1.93.0"
GLFW_TAG = "3.5.1"


def mesa_windows(ctx, dest):
    req = urllib.request.Request("https://api.github.com/repos/pal1000/mesa-dist-win/releases/latest", headers={"User-Agent": "bench"})
    rel = json.load(urllib.request.urlopen(req, timeout=60))
    asset = [a for a in rel["assets"] if a["name"].endswith("release-msvc.7z")][0]
    arc = dest / "mesa.7z"
    urllib.request.urlretrieve(asset["browser_download_url"], arc)
    ex = dest / "mesa"
    ctx.sh(["7z", "x", "-y", "-o" + str(ex), str(arc)])
    return rel["tag_name"], ex / "x64"


def glfw_windows(ctx):
    g, sha = ctx.clone("https://github.com/glfw/glfw.git", GLFW_TAG, name="glfw-src")
    b = ctx.work / "glfw-build"
    ctx.sh(["cmake", "-S", str(g), "-B", str(b), "-G", "MinGW Makefiles", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF", "-DGLFW_BUILD_EXAMPLES=OFF", "-DGLFW_BUILD_TESTS=OFF", "-DGLFW_BUILD_DOCS=OFF", "-DCMAKE_C_COMPILER=" + ctx.cc])
    ctx.sh(["cmake", "--build", str(b), "--config", "Release", "-j", "4"])
    lib = next(b.rglob("libglfw3.a"))
    return g, sha, lib


def real(ctx, src, mine, d, out):
    cflags, link, env, extra = [], [], {}, []
    backends = src / "backends"
    srcs = [src / n for n in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")]
    srcs += [backends / "imgui_impl_glfw.cpp", backends / "imgui_impl_opengl3.cpp"]
    inc = [src, backends, mine]
    if ctx.plat == "linux":
        link = ["-lglfw", "-ldl", "-lpthread"]
        env = {"LIBGL_ALWAYS_SOFTWARE": "1"}
        so = ctx.sh("ldconfig -p | grep 'libglfw.so.3 ' | head -1 | sed 's/.*=> //'").stdout.strip()
        out["glfw_lib_bytes"] = os.path.getsize(os.path.realpath(so))
        out["glfw_source"] = "apt libglfw3-dev, shared libglfw.so.3, GL loaded at runtime from Mesa llvmpipe"
    elif ctx.plat == "macos":
        prefix = ctx.sh(["brew", "--prefix", "glfw"]).stdout.strip()
        inc.append(Path(prefix) / "include")
        link = ["-L" + prefix + "/lib", "-lglfw", "-framework", "Cocoa", "-framework", "OpenGL", "-framework", "IOKit", "-framework", "CoreFoundation"]
        extra = ["-DGL_SILENCE_DEPRECATION"]
        dy = Path(prefix) / "lib" / "libglfw.3.dylib"
        out["glfw_lib_bytes"] = os.path.getsize(os.path.realpath(dy))
        out["glfw_source"] = "brew glfw, shared libglfw.3.dylib, system OpenGL framework"
    else:
        g, sha, lib = glfw_windows(ctx)
        inc.append(g / "include")
        link = [str(lib), "-lgdi32", "-lopengl32"]
        out["glfw_lib_bytes"] = lib.stat().st_size
        out["glfw_source"] = "glfw %s built from source with cmake (MinGW Makefiles), static libglfw3.a (size is the archive, linked into the exe)" % GLFW_TAG
        tag, x64 = mesa_windows(ctx, d)
        out["gl_driver"] = "mesa3d " + tag + " (pal1000/mesa-dist-win) llvmpipe opengl32.dll next to the exe"
        dlls = sorted(x64.rglob("*.dll"))
        for f in dlls:
            shutil.copy(f, d / f.name)
        out["mesa_dlls"] = [f.name for f in dlls]
        env = {"GALLIUM_DRIVER": "llvmpipe"}
    exe = d / ("real" + ctx.exe)
    cmd = [ctx.cxx, "-std=gnu++11", "-Os", "-ffunction-sections", "-fdata-sections"] + extra + ["-I" + str(i) for i in inc]
    cmd += ["-o", str(exe)] + [str(s) for s in srcs + [mine / "real.cpp"]] + cb_common.cxx_static(ctx, True) + cb_common.gc_flags(ctx) + link
    ctx.sh(cmd)
    out["stripped_bytes_hello_real"] = ctx.stripped_size(exe)
    if ctx.plat == "macos":
        out["first_frame_real"] = {"na": "glfwCreateWindow fails on GitHub macOS runners with NSGL: Failed to find a suitable pixel format (tried core 3.2 and default legacy contexts, the VM has no usable OpenGL); on the arm64 runner the attempt also hung for 30 minutes, so it is not run"}
        return
    if ctx.plat == "windows":
        out["first_frame_real"] = {"na": "glfwCreateWindow fails on GitHub Windows runners with WGL: The driver does not appear to support OpenGL (no GPU driver; mesa3d release-msvc 26.2.7 from pal1000/mesa-dist-win ships no opengl32.dll, only vulkan_lvp/dzn dlls)"}
        return
    try:
        out["first_frame_real"] = ctx.first_frame_n([str(exe)], n=5, env=env, cwd=str(d))
    except Exception as e:
        out["first_frame_real"] = {"na": "real window run failed: " + str(e)[-700:]}


def run(ctx):
    src, sha = ctx.clone("https://github.com/ocornut/imgui.git", TAG)
    mine = ctx.bench / "imgui"
    d = ctx.work / "imgui-build"
    d.mkdir(exist_ok=True)
    core = [src / n for n in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")]
    out = {"version": TAG, "commit": sha}
    out["core_loc"] = ctx.loc(core + [src / "imgui.h", src / "imgui_internal.h"])
    out.update(cb_common.null_suite(ctx, "imgui", d, True, [src, mine], core, mine / "hello.cpp", mine / "bench.cpp"))
    out["hello_includes"] = "imgui core (imgui, draw, tables, widgets) + built-in font atlas via io.Fonts->GetTexDataAsRGBA32, no backend, no renderer"
    out["frame_includes"] = "NewFrame + widgets + Render into draw lists (ImDrawData), no rasterization and no backend"
    out["real_includes"] = "imgui core + imgui_impl_glfw + imgui_impl_opengl3 + glfw (see glfw_source); exe size excludes shared glfw/GL libs on Linux and macOS"
    try:
        real(ctx, src, mine, d, out)
    except Exception as e:
        msg = str(e)[-1500:]
        out["stripped_bytes_hello_real"] = {"na": "build failed: " + msg}
        out["first_frame_real"] = {"na": "real window run failed: " + msg}
    out["notes"] = [
        "ImGui is an immediate-mode library that only builds draw lists, the null numbers include no GPU or rasterization work",
        "window is 960x640 for dash; imgui culls vertex generation for rows outside the window clip rect but the widget code still runs for all 50 rows",
        "io.IniFilename is disabled, DeltaTime fixed at 1/60, no mouse input is fed",
        "the Value label is drawn to the right of each slider, ImGui's default",
        "built with -std=gnu++11 -Os (frame_ms) and -O2 (frame_ms_O2) with -ffunction-sections and gc-sections; Windows links libstdc++ and libgcc statically",
        "the real window hello uses the standard GLFW + OpenGL3 backends with swap interval 0 and READY printed after the first SwapBuffers",
    ]
    return out

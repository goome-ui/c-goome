import os, re, shutil, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bench_shared import ldd_sizes, summarize, dir_bytes


def run(ctx):
    src = ctx.bench / "qt"
    d = ctx.work / "qt"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    env = {}
    cmake = ["cmake", "-S", str(src), "-B", str(d / "b"), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=None", "-DCMAKE_CXX_FLAGS=-Os"]
    cxx = ctx.cxx
    if ctx.plat == "macos":
        pre = ctx.sh(["brew", "--prefix", "qt"]).stdout.strip()
        cmake.append("-DCMAKE_PREFIX_PATH=" + pre)
        env["BENCH_LIBDIRS"] = pre + "/lib"
        os.environ["BENCH_LIBDIRS"] = pre + "/lib"
    if ctx.plat == "windows":
        tools = os.environ.get("IQTA_TOOLS", "")
        mingw = None
        for p in Path(tools).glob("mingw*/bin") if tools else []:
            mingw = p
        if mingw:
            cxx = str(mingw / "g++.exe")
            env["PATH"] = str(mingw) + os.pathsep + os.environ["PATH"]
            cmake.append("-DCMAKE_MAKE_PROGRAM=" + shutil.which("ninja"))
        qt = os.environ.get("QT_ROOT_DIR") or os.environ.get("Qt6_DIR")
        if qt:
            cmake.append("-DCMAKE_PREFIX_PATH=" + qt)
    cmake.append("-DCMAKE_CXX_COMPILER=" + cxx)
    ctx.sh(cmake, env=env)
    out = {"version": None}
    out["compile_s_hello"] = ctx.timed(["cmake", "--build", str(d / "b"), "--target", "hello", "--clean-first"], env=env, runs=3)
    ctx.sh(["cmake", "--build", str(d / "b")], env=env)
    hello = d / "b" / ("hello" + ctx.exe)
    bench = d / "b" / ("bench" + ctx.exe)

    run_env = dict(env)
    if ctx.plat == "windows":
        dep = d / "deploy"
        dep.mkdir()
        shutil.copy(hello, dep)
        shutil.copy(bench, dep)
        ctx.sh(["windeployqt", "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", str(dep / hello.name)], env=env)
        for dll in ("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"):
            if not (dep / dll).exists():
                for pdir in run_env.get("PATH", os.environ["PATH"]).split(os.pathsep):
                    if (Path(pdir) / dll).exists():
                        shutil.copy(Path(pdir) / dll, dep)
                        break
        hello = dep / hello.name
        bench = dep / bench.name
        out["deploy_dir_bytes"] = dir_bytes(dep)
        out["qt_dlls_bytes"] = sum(f.stat().st_size for f in dep.rglob("*.dll"))

    out["version"] = ctx.sh([str(hello), "version"], env=run_env).stdout.strip()
    out["commit"] = out["version"]
    out["stripped_bytes_hello_null"] = ctx.stripped_size(hello)
    out["hello_includes"] = "QApplication + QLabel/QPushButton/QVBoxLayout hello, stripped binary only, shared Qt not included, offscreen platform plugin not linked (loaded at run time)"
    out["stripped_bytes_hello_real"] = out["stripped_bytes_hello_null"]
    out["real_includes"] = "same binary; dynamic Qt: see shared_libs_* for the libraries it needs (dynamic linking is how Qt is shipped)"
    if ctx.plat != "windows":
        out.update(summarize(ldd_sizes(ctx, hello), r"Qt"))
        out["shared_libs_matching"] = "library names containing Qt"
        if ctx.plat == "linux":
            out["ldd"] = [x.strip() for x in ctx.sh(["ldd", str(hello)], check=False).stdout.split("\n") if x.strip()]
    else:
        out["shared_libs_bytes_all"] = out["deploy_dir_bytes"]
        out["shared_libs_matching"] = "windeployqt output dir (includes both hello.exe and bench.exe copies and platform/style plugins)"
    out["static_qt"] = {"na": "no static Qt is installable cheaply: apt qt6-base-dev, brew qt and install-qt-action all ship shared builds only, building static Qt from source takes 30+ min and several GB per OS"}
    out["qml_hello"] = {"na": "skipped, Quick would add QtQml/QtQuick and a GPU-less scene graph path, widgets scene only"}

    bt = dict(run_env)
    bt["QT_QPA_PLATFORM"] = "offscreen"
    out["frame_ms_offscreen"] = ctx.frames([str(bench)], env=bt)
    out["frame_includes"] = "widget geometry updates + layout + QWidget::render into QImage ARGB32 via the raster paint engine (software rasterization, offscreen platform), 100 warmup + 2000 frames per scene"
    out["first_frame_offscreen"] = ctx.first_frame_n([str(hello)], n=5, env=bt)
    if ctx.plat != "linux" or os.environ.get("DISPLAY"):
        rt = dict(run_env)
        if ctx.plat == "linux":
            rt["QT_QPA_PLATFORM"] = "xcb"
        try:
            out["first_frame_real"] = ctx.first_frame_n([str(hello)], n=5, env=rt)
        except Exception as e:
            out["first_frame_real"] = {"error": str(e)[-600:]}
    out["bench_src_lines"] = ctx.loc([src / "hello.cpp", src / "bench.cpp"])
    out["notes"] = [
        "dash window grows to the layout minimum size, render target is the widget size",
        "frame bench renders through QWidget::render, not a presented window, no compositor or vsync",
        "compile_s_hello is a cmake/ninja clean rebuild of the hello target only (Qt headers, not Qt itself), best of 3",
    ]
    return out

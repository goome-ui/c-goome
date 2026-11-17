import os
from pathlib import Path


def run(ctx):
    d = ctx.work / "goome"
    d.mkdir(exist_ok=True)
    root = ctx.root
    src = root / "tools/bench/goome"
    inc = ["-I" + str(root)]
    cc = ctx.cc
    out = {"version": None, "header_lines": len((root / "goome.h").read_text().splitlines()), "header_bytes": (root / "goome.h").stat().st_size}
    api = (root / "include/goome.h").read_text()
    import re
    out["api_functions"] = len(set(re.findall(r"\b(goome_[a-z0-9_]+)\(", api)))
    out["api_header_lines"] = len(api.splitlines())
    out["version"] = re.search(r'GOOME_VERSION "([^"]+)"', api).group(1)

    sysl = []
    if ctx.plat == "linux":
        sysl = []
    elif ctx.plat == "macos":
        sysl = []
    elif ctx.plat == "windows":
        sysl = ["-lgdi32", "-luser32", "-lshell32", "-lole32"]

    hello = d / ("hello-Os" + ctx.exe)
    for opt in ("-Os", "-O2"):
        h = d / ("hello" + opt + ctx.exe)
        cmd = [cc, "-std=c99", opt] + inc + ["-o", str(h), str(src / "hello.c")] + sysl
        t = ctx.timed(cmd, runs=3)
        out["compile_s_" + opt[1:]] = t
        out["stripped_bytes_" + opt[1:]] = ctx.stripped_size(h)
    gc = [cc, "-std=c99", "-Os", "-ffunction-sections", "-fdata-sections"] + inc + ["-o", str(d / ("hello_gc" + ctx.exe)), str(src / "hello.c")] + sysl
    gc += ["-Wl,-dead_strip"] if ctx.plat == "macos" else ["-Wl,--gc-sections"]
    ctx.sh(gc)
    out["stripped_bytes_Os_gc"] = ctx.stripped_size(d / ("hello_gc" + ctx.exe))
    hdr_only = d / "hdr.c"
    hdr_only.write_text('#include "goome.h"\nint main(void){return 0;}\n')
    cmd = [cc, "-std=c99", "-Os", "-c"] + inc + ["-o", str(d / "hdr.o"), str(hdr_only)]
    out["compile_s_header_only"] = ctx.timed(cmd, runs=3)
    for tag, defs in (("no_ui", ["-DGOOME_NO_UI"]), ("no_draw", ["-DGOOME_NO_DRAW"])):
        w = d / (tag + ".c")
        w.write_text('#define GOOME_IMPLEMENTATION\n#include "goome.h"\nint main(void){return goome_version_probe();}\n'.replace("goome_version_probe()", "0"))
        b = d / (tag + ctx.exe)
        ctx.sh([cc, "-std=c99", "-Os"] + defs + inc + ["-o", str(b), str(w)] + sysl)
        out["stripped_bytes_Os_" + tag] = ctx.stripped_size(b)
    if ctx.plat == "linux":
        out["ldd"] = [x.strip() for x in ctx.sh(["ldd", str(hello)], check=False).stdout.split("\n") if x.strip()]

    bench = d / ("bench" + ctx.exe)
    ctx.sh([cc, "-std=c99", "-Os"] + inc + ["-o", str(bench), str(src / "bench.c")] + sysl)
    out["frame_ms_offscreen"] = ctx.frames([str(bench), "2000"])
    out["first_frame_offscreen"] = ctx.first_frame_n([str(hello), "offscreen"], n=5)
    ok = ctx.plat != "linux" or os.environ.get("DISPLAY")
    if ok:
        try:
            out["first_frame_window"] = ctx.first_frame_n([str(hello), "window"], n=5)
        except Exception as e:
            out["first_frame_window"] = {"error": str(e)[-600:]}
    return out

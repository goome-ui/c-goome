import os


def gc_flags(ctx):
    return ["-Wl,-dead_strip"] if ctx.plat == "macos" else ["-Wl,--gc-sections"]


def cxx_static(ctx, cxx):
    if cxx and ctx.plat == "windows":
        return ["-static-libgcc", "-static-libstdc++"]
    return []


def compile_cmd(ctx, cxx, opt, std, inc, srcs, out, link=()):
    comp = ctx.cxx if cxx else ctx.cc
    cmd = [comp, std, opt, "-ffunction-sections", "-fdata-sections"] + ["-I" + str(i) for i in inc]
    cmd += ["-o", str(out)] + [str(s) for s in srcs] + cxx_static(ctx, cxx) + gc_flags(ctx) + list(link)
    return cmd


def null_suite(ctx, lib, d, cxx, inc, core, hello, bench, link=()):
    std = "-std=gnu++11" if cxx else "-std=gnu99"
    out = {}
    h = d / ("hello" + ctx.exe)
    cmd = compile_cmd(ctx, cxx, "-Os", std, inc, core + [hello], h, link)
    out["compile_s_hello"] = ctx.timed(cmd, runs=3)
    out["stripped_bytes_hello_null"] = ctx.stripped_size(h)
    ctx.sh([str(h)])
    if ctx.plat == "linux":
        out["ldd"] = [x.strip() for x in ctx.sh(["ldd", str(h)], check=False).stdout.split("\n") if x.strip()]
    for opt, key in (("-Os", "frame_ms"), ("-O2", "frame_ms_O2")):
        b = d / ("bench" + opt[1:] + ctx.exe)
        ctx.sh(compile_cmd(ctx, cxx, opt, std, inc, core + [bench], b, link))
        out[key] = ctx.frames([str(b), "2000"])
    return out

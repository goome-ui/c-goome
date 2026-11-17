import os, re
from pathlib import Path


def ldd_sizes(ctx, exe):
    libs = []
    if ctx.plat == "linux":
        for ln in ctx.sh(["ldd", str(exe)], check=False).stdout.splitlines():
            m = re.search(r"=>\s*(/\S+)", ln)
            if m:
                libs.append(m.group(1))
    elif ctx.plat == "macos":
        seen = set()
        todo = [str(exe)]
        while todo:
            f = todo.pop()
            for ln in ctx.sh(["otool", "-L", f], check=False).stdout.splitlines()[1:]:
                p = ln.strip().split(" (")[0]
                if p.startswith("@rpath/") or p.startswith("@loader_path/"):
                    p = find_rpath(ctx, p.split("/", 1)[1])
                if not p or p in seen or p.startswith("/usr/lib") or p.startswith("/System"):
                    continue
                if os.path.exists(p):
                    seen.add(p)
                    todo.append(p)
        libs = sorted(seen)
    sized = []
    for p in libs:
        r = os.path.realpath(p)
        if os.path.exists(r):
            sized.append((os.path.basename(p), os.path.getsize(r)))
    return sized


def find_rpath(ctx, rel):
    for base in os.environ.get("BENCH_LIBDIRS", "").split(os.pathsep):
        if base and os.path.exists(os.path.join(base, rel)):
            return os.path.join(base, rel)
    return None


def summarize(sized, pat):
    return {
        "shared_libs_count": len(sized),
        "shared_libs_bytes_all": sum(s for _, s in sized),
        "shared_libs_bytes_matching": sum(s for n, s in sized if re.search(pat, n)),
    }


def dir_bytes(d):
    t = 0
    for r, _, fs in os.walk(d):
        for f in fs:
            fp = os.path.join(r, f)
            if not os.path.islink(fp):
                t += os.path.getsize(fp)
    return t

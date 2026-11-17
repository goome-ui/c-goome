import importlib.util, json, os, platform, re, shutil, subprocess, sys, time, traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


class Ctx:
    def __init__(self, work):
        self.work = Path(work).resolve()
        self.work.mkdir(parents=True, exist_ok=True)
        self.root = ROOT
        self.bench = HERE
        s = platform.system()
        self.plat = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}[s]
        self.exe = ".exe" if self.plat == "windows" else ""
        self.cc = os.environ.get("CC") or ("clang" if self.plat == "macos" else "gcc")
        self.cxx = os.environ.get("CXX") or ("clang++" if self.plat == "macos" else "g++")

    def sh(self, cmd, cwd=None, env=None, timeout=1800, check=True):
        e = dict(os.environ)
        e.update(env or {})
        p = subprocess.run(cmd, cwd=cwd, env=e, shell=isinstance(cmd, str), capture_output=True, text=True, timeout=timeout)
        if check and p.returncode:
            raise RuntimeError("%s failed (%d)\n%s\n%s" % (cmd, p.returncode, p.stdout[-2000:], p.stderr[-4000:]))
        return p

    def timed(self, cmd, cwd=None, env=None, runs=3):
        best = None
        for _ in range(runs):
            t = time.perf_counter()
            self.sh(cmd, cwd=cwd, env=env)
            d = time.perf_counter() - t
            best = d if best is None or d < best else best
        return round(best, 3)

    def stripped_size(self, path):
        path = Path(path)
        out = path.with_name(path.stem + "_stripped" + path.suffix)
        shutil.copy(path, out)
        flags = ["-x"] if self.plat == "macos" else []
        self.sh(["strip"] + flags + [str(out)])
        return out.stat().st_size

    def clone(self, url, ref=None, name=None):
        name = name or url.rstrip("/").split("/")[-1].replace(".git", "")
        d = self.work / name
        if d.exists():
            shutil.rmtree(d, ignore_errors=True)
        cmd = ["git", "clone", "--depth", "1"] + (["--branch", ref] if ref else []) + [url, str(d)]
        self.sh(cmd)
        sha = self.sh(["git", "-C", str(d), "rev-parse", "HEAD"]).stdout.strip()
        return d, sha

    def loc(self, files):
        n = 0
        for f in files:
            n += len(Path(f).read_text(errors="replace").splitlines())
        return n

    def first_frame(self, cmd, env=None, cwd=None, settle=1.5, timeout=60):
        import psutil
        e = dict(os.environ)
        e.update(env or {})
        t0 = time.perf_counter()
        p = subprocess.Popen(cmd, cwd=cwd, env=e, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            line = ""
            while True:
                line = p.stdout.readline()
                if line.strip() == "READY" or not line:
                    break
                if time.perf_counter() - t0 > timeout:
                    break
            ms = (time.perf_counter() - t0) * 1000
            if line.strip() != "READY":
                p.kill()
                raise RuntimeError("no READY from %s, stderr: %s" % (cmd, p.stderr.read()[-1500:]))
            time.sleep(settle)
            if p.poll() is not None:
                raise RuntimeError("%s exited early, code %s" % (cmd, p.returncode))
            pr = psutil.Process(p.pid)
            procs = [pr] + pr.children(recursive=True)
            rss = sum(x.memory_info().rss for x in procs)
            uss = None
            try:
                uss = sum(x.memory_full_info().uss for x in procs)
            except Exception:
                pass
            return {"first_frame_ms": round(ms, 1), "rss_mb": round(rss / 1048576, 2), "uss_mb": round(uss / 1048576, 2) if uss else None, "procs": len(procs)}
        finally:
            try:
                for c in psutil.Process(p.pid).children(recursive=True):
                    c.kill()
            except Exception:
                pass
            p.kill()
            p.wait()

    def first_frame_n(self, cmd, n=5, **kw):
        runs = [self.first_frame(cmd, **kw) for _ in range(n)]
        ms = sorted(r["first_frame_ms"] for r in runs)
        out = dict(runs[len(runs) // 2])
        out["first_frame_ms"] = ms[len(ms) // 2]
        out["first_frame_min_ms"] = ms[0]
        out["first_frame_max_ms"] = ms[-1]
        out["runs"] = n
        return out

    def frames(self, cmd, cwd=None, env=None, timeout=600):
        p = self.sh(cmd, cwd=cwd, env=env, timeout=timeout)
        out = {}
        for line in p.stdout.splitlines():
            line = line.strip()
            if line.startswith("{"):
                j = json.loads(line)
                out[j["scene"]] = j["ms_per_frame"]
        if not out:
            raise RuntimeError("no frame json from %s: %s" % (cmd, p.stdout[-500:]))
        return out

    def version(self, cmd):
        try:
            return self.sh(cmd, check=False).stdout.splitlines()[0].strip()
        except Exception:
            return None


def meta(ctx):
    import psutil
    cpu = platform.processor()
    if ctx.plat == "linux":
        m = re.search(r"model name\s*:\s*(.+)", Path("/proc/cpuinfo").read_text())
        cpu = m.group(1) if m else cpu
    elif ctx.plat == "macos":
        cpu = ctx.sh(["sysctl", "-n", "machdep.cpu.brand_string"], check=False).stdout.strip() or cpu
    return {
        "date": time.strftime("%Y-%m-%d"),
        "os": platform.platform(),
        "plat": ctx.plat,
        "runner": os.environ.get("RUNNER_NAME") or os.environ.get("ImageOS") or "local",
        "image": os.environ.get("ImageVersion") or "",
        "arch": platform.machine(),
        "cpu": cpu,
        "cores": psutil.cpu_count(),
        "ram_gb": round(psutil.virtual_memory().total / 2**30, 1),
        "cc": ctx.version([ctx.cc, "--version"]),
        "cxx": ctx.version([ctx.cxx, "--version"]),
        "python": platform.python_version(),
        "display": os.environ.get("DISPLAY"),
        "goome_ref": os.environ.get("GITHUB_SHA") or ctx.sh(["git", "rev-parse", "HEAD"], cwd=ROOT, check=False).stdout.strip(),
    }


def load(name):
    spec = importlib.util.spec_from_file_location("bench_" + name, HERE / "libs" / (name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("libs", nargs="*")
    ap.add_argument("--out", default="bench.json")
    ap.add_argument("--work", default=str(Path.cwd() / "bench-work"))
    a = ap.parse_args()
    ctx = Ctx(a.work)
    libs = a.libs or sorted(p.stem for p in (HERE / "libs").glob("*.py"))
    res = {"meta": meta(ctx), "libs": {}}
    for name in libs:
        t = time.time()
        try:
            res["libs"][name] = load(name).run(ctx)
        except Exception:
            res["libs"][name] = {"error": traceback.format_exc()[-3000:]}
            print("FAILED", name, file=sys.stderr)
            print(res["libs"][name]["error"], file=sys.stderr)
        res["libs"][name]["bench_seconds"] = round(time.time() - t, 1)
        Path(a.out).write_text(json.dumps(res, indent=1))
        print(name, json.dumps(res["libs"][name])[:600], flush=True)


if __name__ == "__main__":
    main()

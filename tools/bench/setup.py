import json, platform, subprocess, sys
from pathlib import Path

here = Path(__file__).resolve().parent
libs = sys.argv[1:] or sorted(p.stem for p in (here / "libs").glob("*.py"))
plat = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}[platform.system()]
apt, brew, choco, pip = [], [], [], []
for n in libs:
    f = here / "libs" / (n + ".deps.json")
    if not f.exists():
        continue
    d = json.loads(f.read_text())
    apt += d.get("apt", [])
    brew += d.get("brew", [])
    choco += d.get("choco", [])
    pip += d.get("pip", [])


def run(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


if pip:
    run([sys.executable, "-m", "pip", "install"] + sorted(set(pip)))
if plat == "linux" and apt:
    run(["sudo", "apt-get", "update"])
    run(["sudo", "apt-get", "install", "-y", "--no-install-recommends"] + sorted(set(apt)))
if plat == "macos" and brew:
    run(["brew", "install"] + sorted(set(brew)))
if plat == "windows" and choco:
    run(["choco", "install", "-y", "--no-progress"] + sorted(set(choco)))

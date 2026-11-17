import shutil
import subprocess
import urllib.request
from pathlib import Path

VERSION = "26.2.7"
URL = "https://github.com/pal1000/mesa-dist-win/releases/download/%s/mesa3d-%s-release-msvc.7z" % (VERSION, VERSION)


def fetch(ctx):
    d = ctx.work / "mesa"
    marker = d / "x64" / "opengl32.dll"
    if marker.exists():
        return d / "x64"
    d.mkdir(parents=True, exist_ok=True)
    arc = d / "mesa.7z"
    urllib.request.urlretrieve(URL, arc)
    sevenz = shutil.which("7z") or r"C:\Program Files\7-Zip\7z.exe"
    subprocess.run([sevenz, "x", "-y", "-o" + str(d), str(arc)], check=True, capture_output=True)
    return d / "x64"


def install(ctx, exe_dir):
    src = fetch(ctx)
    for f in ("opengl32.dll", "libgallium_wgl.dll", "libglapi.dll"):
        p = src / f
        if p.exists():
            shutil.copy(p, Path(exe_dir) / f)
    return {"mesa_dist_win": VERSION}

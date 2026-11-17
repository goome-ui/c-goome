import json, re, subprocess, sys

repo = "goome-ui/c-goome"
runs = json.loads(subprocess.check_output(["gh", "run", "list", "-R", repo, "--workflow=ci.yml", "--branch", "dev", "--status", "success", "--limit", "1", "--json", "databaseId,headSha,url,createdAt"]))
run = runs[0]
meta = json.loads(subprocess.check_output(["gh", "run", "view", str(run["databaseId"]), "-R", repo, "--json", "jobs"]))
log = subprocess.check_output(["gh", "run", "view", str(run["databaseId"]), "-R", repo, "--log"], text=True, errors="replace")
ansi = re.compile(r"\x1b\[[0-9;]*m")
by_job = {}
for line in log.splitlines():
    parts = line.split("\t", 2)
    if len(parts) < 3:
        continue
    text = ansi.sub("", parts[2])
    text = re.sub(r"^﻿?\d{4}-\d\d-\d\dT[\d:.]+Z ?", "", text)
    by_job.setdefault(parts[0], []).append(text)

def section(lines, start, stop="##[group]"):
    out, on = [], False
    for t in lines:
        if t.startswith("##[group]"):
            if on:
                break
            on = start in t
            continue
        if on:
            out.append(t)
    return out

def count(lines, pat):
    r = re.compile(pat)
    return sum(1 for t in lines if r.match(t))

facts = {"run": run, "jobs": {}}
for j in meta["jobs"]:
    name = j["name"]
    lines = by_job.get(name, [])
    img = ver = None
    for i, t in enumerate(lines):
        if t.strip() == "Image: " + t.strip()[7:] and t.startswith("Image:"):
            img = t.split(":", 1)[1].strip()
            ver = lines[i + 1].split(":", 1)[1].strip()
            break
    info = {"conclusion": j["conclusion"], "image": img, "image_version": ver, "steps": {}}
    if "all good" in lines and name in ("linux", "windows") or name.startswith("macos"):
        info["unit"] = "all good" if "all good" in lines else "no all good line"
    if name == "linux":
        for t in lines:
            m = re.match(r"passed (\d+), failed (\d+)$", t)
            if m:
                info["steps"]["x11.sh"] = {"pass": int(m[1]), "fail": int(m[2])}
            m = re.match(r"(\d+) passed, (\d+) failed$", t)
            if m:
                info["steps"]["wayland.sh"] = {"pass": int(m[1]), "fail": int(m[2])}
            m = re.match(r"backends: (\d+) ran, (\d+) failed", t)
            if m:
                info["steps"]["backends.sh"] = {"ran": int(m[1]), "fail": int(m[2])}
        at = section(lines, "at-spi2-core")
        info["steps"]["atspi.sh"] = {"ok": count(at, r"ok   "), "fail": count(at, r"FAIL "), "final": "atspi ok" in at}
    if name == "windows":
        u = section(lines, "uia.ps1")
        info["steps"]["uia.ps1"] = {"ok": count(u, r"ok   "), "fail": count(u, r"FAIL ")}
        w = section(lines, "win32.ps1")
        info["steps"]["win32.ps1"] = {"pass": count(w, r"PASS "), "fail": count(w, r"FAIL "), "final": "win32 desktop: all passed" in w}
    if name.startswith("macos"):
        m = section(lines, "tests/macos.sh")
        info["steps"]["macos.sh"] = {"pass": count(m, r"PASS "), "fail": count(m, r"FAIL ")}
        a = section(lines, "tests/macax.sh")
        info["steps"]["macax.sh"] = {"ok": count(a, r"ok   "), "fail": count(a, r"FAIL "), "self_check": any("self ok" in t or "no ax client" in t for t in a)}
    facts["jobs"][name] = info
json.dump(facts, open(sys.argv[1] if len(sys.argv) > 1 else "ci-facts.json", "w"), indent=1)
print(json.dumps(facts, indent=1))

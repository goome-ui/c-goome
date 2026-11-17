import glob, json, os, sys

root = sys.argv[1]
rows = []
for p in sorted(glob.glob(os.path.join(root, "**", "results.jsonl"), recursive=True)):
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if line:
            rows.append(json.loads(line))
rows.sort(key=lambda r: r["cell"])
json.dump(rows, open(sys.argv[2], "w"), indent=1, ensure_ascii=False)
print(len(rows), "results")
for r in rows:
    print("%-5s %-55s %s" % (r["result"], r["cell"], r["detail"][:160].replace("\n", " ")))

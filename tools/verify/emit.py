import json, os, sys

cell, ran, result, detail = sys.argv[1:5]
version = sys.argv[5] if len(sys.argv) > 5 else ""
out = os.environ.get("VERIFY_OUT", "verify-out")
os.makedirs(out, exist_ok=True)
row = {"cell": cell, "ran": ran in ("1", "true", "yes"), "result": result, "detail": detail[:3000]}
if version:
    row["version"] = version
with open(os.path.join(out, "results.jsonl"), "a") as f:
    f.write(json.dumps(row) + "\n")
print("RESULT", json.dumps(row))

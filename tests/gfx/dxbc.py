import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[2]
cso = pathlib.Path(sys.argv[1])
blobs = {
    "goome_dx11.h": {"gm_dx11_vs": "vs_4_0", "gm_dx11_ps": "ps_4_0"},
    "goome_dx12.h": {"gm_dx12_vs": "vs_5_0", "gm_dx12_ps": "ps_5_0"},
}

for name, arrays in blobs.items():
    path = root / "backends" / name
    if not path.exists():
        continue
    text = path.read_text()
    for var, prof in arrays.items():
        data = (cso / (prof + ".cso")).read_bytes()
        rows = ["    " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + "," for i in range(0, len(data), 16)]
        body = "static const unsigned char %s[] = {\n%s\n};" % (var, "\n".join(rows))
        text, n = re.subn(r"static const unsigned char %s\[\] = \{.*?\n\};" % var, lambda m: body, text, flags=re.S)
        if n != 1:
            sys.exit("%s: no %s array" % (name, var))
    path.write_text(text)

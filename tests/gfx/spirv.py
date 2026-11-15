import pathlib
import re
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
header = here.parents[1] / "backends" / "goome_vulkan.h"
glslang = sys.argv[1] if len(sys.argv) > 1 else "glslang"
text = header.read_text()
for stage, var in (("vert", "gm_vk_vs"), ("frag", "gm_vk_fs")):
    out = here / ("quad." + stage + ".spv")
    subprocess.run([glslang, "-V", "-g0", "--quiet", "-o", str(out), str(here / ("quad." + stage))], check=True)
    data = out.read_bytes()
    out.unlink()
    words = [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]
    rows = ["    " + ", ".join("0x%08x" % w for w in words[i:i + 8]) + "," for i in range(0, len(words), 8)]
    body = "static const uint32_t %s[] = {\n%s\n};" % (var, "\n".join(rows))
    text, n = re.subn(r"static const uint32_t %s\[\] = \{.*?\n\};" % var, lambda m: body, text, flags=re.S)
    if n != 1:
        sys.exit("no %s array" % var)
header.write_text(text)

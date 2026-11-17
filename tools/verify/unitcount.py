import sys

src = open("tests/unit.c", encoding="utf-8").read()
src = src.replace("#define CHECK(x)", "static long checks;\n#define CHECK(x)", 1)
src = src.replace('if (!(x)) printf("FAIL', 'checks++; if (!(x)) printf("FAIL', 1)
tail = 'printf(fails ? "%d failed\\n" : "all good\\n", fails);'
assert tail in src
src = src.replace(tail, 'printf("CHECKS %ld\\n", checks);\n    ' + tail, 1)
open(sys.argv[1], "w", encoding="utf-8").write(src)

import sys
kv = {}
with open(sys.argv[1], encoding="utf-8") as f:
    for line in f:
        k, _, v = line.strip().partition(" ")
        if k:
            kv[k] = v
ver = f"{kv.get('STABLE_README_VERSION','')}+{kv.get('STABLE_SCM_REVISION','')}{kv.get('STABLE_SCM_DIRTY','')}"
with open(sys.argv[2], "w", encoding="ascii") as out:
    out.write(ver)
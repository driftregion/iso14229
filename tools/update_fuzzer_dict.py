#!/usr/bin/env python3
import glob
import re
from pathlib import Path

# --- collect lines from all fuzz-*.log files ---
log_files = sorted(glob.glob("fuzz-*.log"))
lines = []
for log_file in log_files:
    with open(log_file, "r", encoding="utf-8", errors="replace") as f:
        in_dict = False
        for line in f:
            if line.startswith("###### Recommended dictionary. ######"):
                in_dict = True
                continue
            if line.startswith("###### End of recommended dictionary. ######"):
                in_dict = False
                continue
            if in_dict:
                lines.append(line.rstrip("\n"))

# strip comments (" # ") and surrounding quotes
lines = [l.split(" # ")[0].strip().strip('"') for l in lines]
lines = [l for l in lines if l]  # drop empties

# --- parse backslash-octal sequences and emit hex-escaped strings ---
# e.g. r"\007\000\000\000\000\000\000\000" -> "\\x07\\x00\\x00\\x00\\x00\\x00\\x00\\x00"
OCT_RE = re.compile(r'\\([0-7]{1,3})')  # 1–3 octal digits per escape

hex_lines = []
for line in lines:
    oct_groups = OCT_RE.findall(line)
    if not oct_groups:
        # no octal escapes found; skip or keep raw (choose one)
        # hex_lines.append(line)   # keep raw
        continue                   # or skip

    b = bytes(int(o, 8) for o in oct_groups)     # convert to bytes
    hex_line = "".join(f"\\x{byte:02X}" for byte in b)
    hex_lines.append(hex_line)

# --- append to fuzz/dictionary.txt if missing ---
dict_path = Path("fuzz/dictionary.txt")
dict_path.parent.mkdir(parents=True, exist_ok=True)
existing = set()
if dict_path.exists():
    existing = {ln.strip().strip('"') for ln in dict_path.read_text(encoding="utf-8").splitlines() if ln.strip()}

new_entries = []
with dict_path.open("a", encoding="utf-8") as f:
    for line in hex_lines:
        if line not in existing:
            f.write(f'"{line}"\n')
            new_entries.append(line)

print(f"Added {len(new_entries)} new entries to the dictionary.")

# show results
for i, line in enumerate(new_entries):
    print(f"{i}: {line}")

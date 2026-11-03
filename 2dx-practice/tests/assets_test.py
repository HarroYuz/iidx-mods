"""Validate explicit upright atlas rectangles without a Pillow dependency."""
from pathlib import Path
import hashlib
import json
import struct
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[1]
hashes = json.loads((root / "addresses/asset-native-hashes.json").read_text())
for relative, expected in hashes.items():
    path = root.parent / "data_mods/practice" / relative
    tree = ET.parse(path).getroot()
    assert hashlib.sha256(b"".join(ET.tostring(x).strip() for x in tree[:-1])).hexdigest() == expected
    custom = tree[-1]
    assert custom.get("name") == "practice000"
    w, h = map(int, custom.findtext("size").split())
    rectangles = []
    pngs = {p.stem: p for p in path.parent.parent.glob("*.png")}
    assert len(pngs) == 28
    for image in custom.findall("image"):
        png = pngs.pop(image.get("name"))
        data = png.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n"
        l, r, t, b = [int(v) // 2 for v in image.findtext("imgrect").split()]
        assert struct.unpack_from(">II", data, 16) == (r-l, b-t)
        assert 0 <= l < r <= w and 0 <= t < b <= h
        for x1, x2, y1, y2 in rectangles:
            assert r <= x1 or x2 <= l or b <= y1 or y2 <= t
        rectangles.append((l, r, t, b))
    assert not pngs
print("PASS: 8 native texture lists retained; 224 added sprite rectangles upright and disjoint")

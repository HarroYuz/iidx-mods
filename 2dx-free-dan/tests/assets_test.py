from pathlib import Path
import struct
root = Path(__file__).resolve().parents[1]
for folder, count in (("assets", 5), ("assets-0917", 10)):
    files = list((root / folder).rglob("*.png"))
    assert len(files) == count
    for path in files:
        data = path.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n"
        assert all(0 < n <= 2048 for n in struct.unpack_from(">II", data, 16))
assert not list(root.glob("assets*/**/opt_select*"))
print("PASS: separate Dan sprites for both profiles; no practice asset collisions")

for target in ("assets/graphic/1", "assets-0917/graphic/0", "assets-0917/graphic/1"):
    sprites = root/target/"mselect_ifs"
    expected = {"ss_self_dan": (546,64), "ssm_self_dan": (400,160), "so_self_dan": (200,38)}
    blobs=[]
    for name,size in expected.items():
        blob=(sprites/f"{name}.png").read_bytes();blobs.append(blob)
        assert struct.unpack_from(">II",blob,16)==size
        assert blob[25]==6 # RGBA PNG, required for native texture compositing.
    assert len(set(blobs))==3
print("PASS: distinct entry logo, list title and 64px subtitle across both generations")

"""Read-only audit of the actual optional DLL and its embedded game profiles."""
from pathlib import Path
import argparse, hashlib, json, re, struct

class PE:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        self.pe = struct.unpack_from('<I', self.data, 60)[0]
        self.opt = self.pe + 24
        self.base = struct.unpack_from('<Q', self.data, self.opt + 24)[0]
        start = self.opt + struct.unpack_from('<H', self.data, self.pe + 20)[0]
        self.sections = [struct.unpack_from('<IIII', self.data, start + i*40 + 8)
                         for i in range(struct.unpack_from('<H', self.data, self.pe + 6)[0])]
    def read(self, rva, size):
        for _, va, raw, offset in self.sections:
            if va <= rva and rva + size <= va + raw:
                return self.data[offset+rva-va:offset+rva-va+size]
        raise ValueError(f'RVA outside file-backed sections: {rva:#x}')

root = Path(__file__).resolve().parents[1]
args = argparse.ArgumentParser()
args.add_argument('--game-root', required=True)
args = args.parse_args()
game_root = Path(args.game_root)
manifest = json.loads((root/'addresses/firenet-91067758.json').read_text(encoding='utf-8'))
fire, game = PE(game_root/'firenet.dll'), PE(game_root/'bm2dx.dll')
assert hashlib.sha256(fire.data).hexdigest() == manifest['sha256'], 'Unreviewed FireNet DLL'
game_hash = hashlib.sha256(game.data).hexdigest()
profiles = []
for match in re.finditer(rb'\{\s*"schema_version":', fire.data):
    profile, _ = json.JSONDecoder().raw_decode(fire.data[match.start():].decode('utf-8', errors='replace'))
    if profile['identity']['sha256'] == game_hash:
        profiles.append(profile)
assert len(profiles) == 1, 'Expected one matching embedded profile'
profile = profiles[0]
checked_bytes = checked_pointers = 0
for feature in profile['features'].values():
    if not feature.get('available'):
        continue
    for check in feature.get('checks', []):
        expected = bytes.fromhex(check['bytes'])
        assert game.read(check['rva'], len(expected)) == expected, f'Byte mismatch {check["rva"]:#x}'
        checked_bytes += 1
    for check in feature.get('pointers', []):
        assert struct.unpack('<Q', game.read(check['rva'], 8))[0] == game.base + check['target']
        checked_pointers += 1
expected = bytes.fromhex(manifest['dan_exit_bytes'])
assert fire.read(int(manifest['dan_exit'], 16), len(expected)) == expected
recorder = profile['features']['recorder']
for callback in manifest['scene_callbacks']:
    name, slot = callback['name'], callback['slot']
    address = recorder['rvas'][name] + slot*8
    assert struct.unpack('<Q', game.read(address, 8))[0] == game.base + recorder['lists'][name][slot]
report = dict(firenet=manifest['version'], sha256=manifest['sha256'], game_sha256=game_hash,
              byte_checks=checked_bytes, pointer_checks=checked_pointers,
              scene_callbacks=manifest['scene_callbacks'], limits=manifest['limits'])
(root/'build').mkdir(exist_ok=True)
(root/'build/firenet-audit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(f'PASS: actual FireNet identity, matching embedded profile, {checked_bytes} byte checks, '
      f'{checked_pointers} pointer checks and 6 recorder scene slots')

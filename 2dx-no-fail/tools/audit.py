"""Read-only identification of the installed game and both GSM providers.

No loading DLLs, executing game code, patching files or accessing the network.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

EXPECTED = {
    'bm2dx.dll': '3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83',
    '2dx-gsm.dll': 'ecf5d28b283b7dadd1930c012c77963b9094f54c666d5bd37fc291fb9ec46823',
    'firenet.dll': '9de791ab4d0b5bf9a7daafa00a75abe4bd1155a8c053717f6f349dd8c57a93a2',
}

def read_rva(data, rva, size):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    for i in range(count):
        section = pe + 24 + optional_size + 40*i
        _, va, raw_size, raw = struct.unpack_from('<IIII', data, section + 8)
        if va <= rva and rva + size <= va + raw_size:
            return data[raw + rva - va:raw + rva - va + size]
    raise ValueError(f'Unbacked RVA {rva:#x}')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    files = {name: (args.game_root / name).read_bytes() for name in EXPECTED}
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    if hashes != EXPECTED:
        raise RuntimeError('Unreviewed binary identity: ' + json.dumps(hashes))
    profiles = []
    fire = files['firenet.dll']
    for match in re.finditer(rb'\{\s*"schema_version":', fire):
        profile, _ = json.JSONDecoder().raw_decode(
            fire[match.start():].decode('utf-8', errors='replace'))
        if profile['identity']['sha256'] == hashes['bm2dx.dll']:
            profiles.append(profile)
    if len(profiles) != 1:
        raise RuntimeError('Expected exactly one FireNet profile for this game')
    gsm = profiles[0]['features']['gsm']
    if not gsm['available']:
        raise RuntimeError('FireNet GSM unavailable')
    for check in gsm['checks']:
        expected = bytes.fromhex(check['bytes'])
        if read_rva(files['bm2dx.dll'], check['rva'], len(expected)) != expected:
            raise RuntimeError(f'FireNet GSM signature mismatch: {check["rva"]:#x}')
    # Independently inspected native accumulator: skips addition when <=0,
    # then treats <100 units (2%) as depleted. These are NOT patch recipes.
    sites = {
        'native_skip_nonpositive': (0x95cf9c, '85c97e08'),
        'native_low_gauge_threshold': (0x95cfb0, '83f964'),
        'native_hard_clear_positive': (0x95c3cf, '41833e007e06'),
    }
    for name, (rva, expected) in sites.items():
        if read_rva(files['bm2dx.dll'], rva, len(bytes.fromhex(expected))) != bytes.fromhex(expected):
            raise RuntimeError(f'Native site mismatch: {name}')
    report = {
        'sha256': hashes,
        'firenet_gsm_checks': len(gsm['checks']),
        'firenet_gsm': gsm,
        'native_observations': {k: {'rva': hex(v[0]), 'bytes': v[1]} for k,v in sites.items()},
        'status': 'Identification only. No runtime adapter installed or validated.',
        'limits': ['Does not test hook chaining or load order.',
                   'Does not validate score upload or negative graph rendering.'],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: three file identities, {len(gsm["checks"])} FireNet GSM signatures, three native sites')

if __name__ == '__main__':
    main()

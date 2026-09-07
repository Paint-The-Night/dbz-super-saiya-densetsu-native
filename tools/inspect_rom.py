#!/usr/bin/env python3
"""Read-only identification of this project's exact Japanese Rev 1 ROM."""
import argparse
import hashlib
import json
from pathlib import Path
import zlib

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / 'Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc'
EXPECTED_SHA256 = '962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c'


def inspect(path):
    raw = path.read_bytes()
    header_size = 512 if len(raw) % 32768 == 512 else 0
    data = raw[header_size:]
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError('Not the pinned Rev 1 ROM; do not reuse its addresses for another revision.')
    h = data[0x7FC0:0x8000]
    word = lambda offset: int.from_bytes(h[offset:offset + 2], 'little')
    vectors = {name: f'00:{word(offset):04X}' for name, offset in (
        ('native_cop', 0x24), ('native_brk', 0x26), ('native_abort', 0x28),
        ('native_nmi', 0x2A), ('native_irq', 0x2E), ('emulation_reset', 0x3C))}
    return {
        'file': str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
        'file_size_bytes': len(raw), 'copier_header_bytes': header_size,
        'rom_size_bytes': len(data), 'sha256': digest,
        'sha1': hashlib.sha1(data).hexdigest(), 'crc32': f'{zlib.crc32(data):08x}',
        'internal_title': h[:21].decode('ascii').rstrip(),
        'map_mode': f'0x{h[21]:02x}', 'mapping': 'LoROM',
        'chipset': f'0x{h[22]:02x}', 'cartridge_type': 'ROM + RAM + battery',
        'enhancement_chip_indicated': False,
        'declared_rom_bytes': 1024 << h[23],
        'declared_sram_bytes': 0 if h[24] == 0 else 1024 << h[24],
        'region_byte': h[25], 'revision_byte': h[27],
        'checksum_stored': f'{word(0x1E):04x}',
        'checksum_calculated': f'{sum(data) & 65535:04x}',
        'checksum_matches': (sum(data) & 65535) == word(0x1E),
        'checksum_complement_matches': (word(0x1C) ^ word(0x1E)) == 65535,
        'lorom_32k_banks': len(data) // 32768, 'vectors': vectors,
        'limitations': 'Header and identity inspection only; no execution or decompilation coverage measured.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', nargs='?', type=Path, default=ROM)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.rom.resolve()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')

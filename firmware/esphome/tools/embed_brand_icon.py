#!/usr/bin/env python3
"""Embed the project PNG in the HTTPS firmware; --check verifies reproducibility."""
import argparse
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
root=Path(__file__).resolve().parents[1];data=(root/'assets/sucroclock-icon-192.png').read_bytes()
if not data.startswith(b'\x89PNG\r\n\x1a\n') or len(data)>65536:parser.error('Icon must be a PNG no larger than 64 KiB')
text='#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace esphome::clock_setup {\n// Generated SucroClock icon, resized to 192px; source in assets/.\nstatic const uint8_t BRAND_ICON[] = {\n'+''.join('  '+','.join(f'0x{x:02x}' for x in data[i:i+16])+',\n' for i in range(0,len(data),16))+'};\nstatic constexpr size_t BRAND_ICON_SIZE=sizeof(BRAND_ICON);\n}\n'
target=root/'components/clock_setup/brand_icon.h'
if args.check:
 if target.read_text()!=text:parser.error('Embedded icon differs; run this script without --check')
 print('Embedded icon matches the source PNG.')
else:target.write_text(text)

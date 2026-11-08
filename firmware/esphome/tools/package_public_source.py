#!/usr/bin/env python3
"""Create a source-only SucroClock archive from an explicit project allowlist."""
import argparse
import hashlib
import re
from pathlib import Path
import zipfile
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
repo=Path(__file__).resolve().parents[3]
roots=['README.md','CHANGELOG.md','LICENSE','NOTICE','.gitignore','docs','reference','firmware/esphome/README.md','firmware/esphome/DEXCOM.md','firmware/esphome/.gitignore','firmware/esphome/clock.yaml','firmware/esphome/clock-core.yaml','firmware/esphome/secrets.yaml.example','firmware/esphome/requirements-build.txt','firmware/esphome/components','firmware/esphome/packages','firmware/esphome/tools','firmware/esphome/tests','firmware/esphome/examples','firmware/esphome/docs','firmware/esphome/assets']
blocked={'.git','.clock-private','.esphome','.venv','venv','__pycache__'}
suffixes={'.md','.txt','.py','.h','.cpp','.yaml','.yml','.json','.png','.jpg','.jpeg','.gif','.svg','.pdf','.sh'}
external_references={'docs/images/schematic-hu058d.jpg','reference/AiP33628_icbase.pdf','firmware/esphome/assets/sucroclock-icon-original.png'}
files={}
for item in roots:
 path=repo/item
 for p in ([path] if path.is_file() else path.rglob('*')):
  rel=p.relative_to(repo)
  if str(rel) in external_references:continue
  if not p.is_file() or p.is_symlink() or any(part in blocked for part in rel.parts):continue
  if p.name=='secrets.yaml' or p.suffix in {'.bin','.elf','.pem','.key','.crt','.pyc','.swp','.swo'}:continue
  if p.name not in {'LICENSE','NOTICE','.gitignore','secrets.yaml.example'} and p.suffix not in suffixes:continue
  body=p.read_bytes()
  if re.search(rb'-----BEGIN (?:EC |RSA )?PRIVATE KEY-----\r?\n',body):parser.error('Private key material found in '+str(rel))
  files[str(rel)]=body
if args.output.resolve().is_relative_to(repo):parser.error('Write the release archive outside the working repository')
args.output.parent.mkdir(parents=True,exist_ok=True)
manifest=''.join(hashlib.sha256(body).hexdigest()+'  '+path+'\n' for path,body in sorted(files.items()))
with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED) as archive:
 for path,body in sorted(files.items()):archive.writestr('SucroClock/'+path,body)
 archive.writestr('SucroClock/SOURCE-MANIFEST.sha256',manifest)
print(f'Created source-only archive: {len(files)} files; device settings, certificates, keys and binaries excluded.')

#!/usr/bin/env python3
"""Package the Mac prototype with SDL, attribution, and no game data."""
from pathlib import Path
import hashlib
import json
import plistlib
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'dist/DBZ Native Port.app'


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


if sys.platform != 'darwin':
    raise SystemExit('This packaging step is macOS-only; the CMake executable is portable.')
run('cmake', '--build', ROOT / 'build', '--parallel', '4')
binary = ROOT / 'build/dbz-port'
deps = subprocess.check_output(['otool', '-L', str(binary)], text=True).splitlines()[1:]
sdl = next(Path(line.strip().split(' (')[0]) for line in deps if 'libSDL2' in line)
def minimum_macos(path):
    commands = subprocess.check_output(['otool', '-l', str(path)], text=True)
    versions = re.findall(r'\bminos\s+([0-9.]+)', commands)
    if not versions:
        raise SystemExit(f'Cannot determine minimum macOS for {path}')
    return max(versions, key=lambda v: tuple(map(int, v.split('.'))))

minimum = max((minimum_macos(binary), minimum_macos(sdl)),
              key=lambda v: tuple(map(int, v.split('.'))))
# Reject additional non-system dylibs rather than shipping an incomplete bundle.
for dep in subprocess.check_output(['otool', '-L', str(sdl)], text=True).splitlines()[2:]:
    name = dep.strip().split(' (')[0]
    if not name.startswith(('/usr/lib/', '/System/Library/')):
        raise SystemExit(f'Unhandled SDL dependency: {name}')
macos = APP / 'Contents/MacOS'
frameworks = APP / 'Contents/Frameworks'
resources = APP / 'Contents/Resources'
for directory in (macos, frameworks, resources):
    directory.mkdir(parents=True, exist_ok=True)
shutil.copy2(binary, macos / 'dbz-port')
# Homebrew's dylib is read-only; replace the previous bundled copy on rebuild.
(frameworks / sdl.name).unlink(missing_ok=True)
shutil.copy2(sdl.resolve(), frameworks / sdl.name)
run('install_name_tool', '-change', sdl, f'@executable_path/../Frameworks/{sdl.name}', macos / 'dbz-port')
run('install_name_tool', '-id', f'@rpath/{sdl.name}', frameworks / sdl.name)
shutil.copy2(ROOT / 'third_party/lakesnes/LICENSE.txt', resources / 'LakeSnes-LICENSE.txt')
shutil.copy2(sdl.parent.parent / 'LICENSE.txt', resources / 'SDL2-LICENSE.txt')
shutil.copy2(ROOT / 'docs/prototype.md', resources / 'README.md')
shutil.copy2(ROOT / 'docs/ram-map.md', resources / 'ram-map.md')
info = {
    'CFBundleExecutable': 'dbz-port', 'CFBundleIdentifier': 'local.garyperrigo.dbznativeport',
    'CFBundleName': 'DBZ Native Port', 'CFBundleDisplayName': 'DBZ Native Port',
    'CFBundlePackageType': 'APPL', 'CFBundleShortVersionString': '0.1.0',
    'CFBundleVersion': '1', 'NSHighResolutionCapable': True,
    'LSMinimumSystemVersion': minimum,
}
with (APP / 'Contents/Info.plist').open('wb') as f:
    plistlib.dump(info, f)
run('codesign', '--force', '--sign', '-', frameworks / sdl.name)
run('codesign', '--force', '--sign', '-', APP)
run('codesign', '--verify', '--deep', '--strict', APP)
manifest = {str(p.relative_to(APP)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(APP.rglob('*')) if p.is_file()}
(ROOT / 'artifacts').mkdir(exist_ok=True)
(ROOT / 'artifacts/package-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(APP)

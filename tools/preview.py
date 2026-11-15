"""Render the real home screen and buddy art on a PC: python tools/preview.py

Compiles tools/preview.cpp against the same drawing headers the firmware uses and writes PNGs to
build/preview/. Needs a C++ compiler, Pillow, and the display font that `pio run` downloads."""
from pathlib import Path
import os, re, shlex, shutil, subprocess, sys
from PIL import Image
root = Path(__file__).resolve().parents[1]
os.chdir(root)
out = root / 'build' / 'preview'
out.mkdir(parents=True, exist_ok=True)
fonts = sorted(root.glob('.pio/libdeps/*/M5GFX*/src/lgfx/Fonts/glcdfont.h'))
if not fonts:
    sys.exit('Display font not found. Run `pio run -e stackchan` once so PlatformIO fetches M5GFX.')
table = fonts[0].read_text().split('{', 1)[1].split('}', 1)[0]
data = re.findall(r'0x[0-9A-Fa-f]{2}', table)
assert len(data) >= 256 * 5, len(data)
(out / 'font.cpp').write_text('extern const unsigned char previewFont[] = {' + ','.join(data) + '};\n')
cxx = shlex.split(os.environ['CXX']) if os.environ.get('CXX') else [shutil.which('g++') or 'c++']
exe = out / 'preview.exe'
subprocess.run(cxx + ['-std=c++17', '-O1', '-Iinclude', '-Itools', 'tools/preview.cpp', str(out / 'font.cpp'),
                      '-o', str(exe)], check=True)
subprocess.run([str(exe), str(out)], check=True)
publish = '--site' in sys.argv  # Also refresh the documentation screenshots in site/assets.
for ppm in sorted(out.glob('*.ppm')):
    image = Image.open(ppm)
    target = ppm.with_suffix('.png')
    if ppm.name.startswith('ui-'):
        if publish:
            image.save(root / 'site' / 'assets' / target.name)
    else:
        scale = 1 if image.width > 640 else 2
        image = image.resize((image.width * scale, image.height * scale), Image.NEAREST)
    image.save(target)
    ppm.unlink()
    print(target.relative_to(root))

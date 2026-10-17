"""Run after pio build: exercise our framer with the actual pinned GPS parser."""
from pathlib import Path
import os, shlex, shutil, subprocess
root = Path(__file__).resolve().parents[1]
os.chdir(root)
if os.environ.get('CXX'):
    cxx = shlex.split(os.environ['CXX'])
elif shutil.which('g++'):
    cxx = ['g++']
else:
    import ziglang
    cxx = [str(Path(ziglang.__file__).parent / ('zig.exe' if os.name=='nt' else 'zig')), 'c++']
gps = Path('.pio/libdeps/stackchan/TinyGPSPlus/src')
assert (gps/'TinyGPS++.cpp').exists(), 'Build firmware first to resolve its pinned GPS dependency.'
Path('build').mkdir(exist_ok=True)
subprocess.run(cxx + ['-std=c++17', '-Iinclude', '-Itests/host', '-I'+str(gps),
                     'tests/gps_parser_test.cpp', str(gps/'TinyGPS++.cpp'), '-o', 'build/gps_parser_test.exe'], check=True)
subprocess.run([str(root/'build/gps_parser_test.exe')], check=True)

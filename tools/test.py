"""Host regression checks; uses CXX or a pip-installed ziglang compiler."""
from pathlib import Path
import csv, os, shlex, shutil, subprocess, sys
root = Path(__file__).resolve().parents[1]
os.chdir(root)
Path('build').mkdir(exist_ok=True)
if os.environ.get('CXX'):
    cxx = shlex.split(os.environ['CXX']); cc = shlex.split(os.environ.get('CC', 'cc'))
elif shutil.which('g++'):
    cxx = ['g++']; cc = ['gcc']
else:
    import ziglang
    zig = str(Path(ziglang.__file__).parent / ('zig.exe' if os.name=='nt' else 'zig'))
    cxx = [zig, 'c++']; cc = [zig, 'cc']
includes = ['-Iinclude', '-Ilib/opendroneid/src']
subprocess.run(cc + includes + ['-DODID_DISABLE_PRINTF','-c','lib/opendroneid/src/opendroneid.c','-o','build/odid.o'],check=True)
subprocess.run(cxx + ['-std=c++17','-O1','-g'] + includes + ['tests/core_test.cpp','build/odid.o','-o','build/core_test.exe'],check=True)
subprocess.run([str(root/'build/core_test.exe')],check=True)
with open('build/test-wigle.csv', newline='') as f:
    assert f.readline().startswith('WigleWifi-1.4,')
    rows=list(csv.DictReader(f))
assert len(rows)==2 and all(len(r)==11 and None not in r for r in rows)
assert rows[0]['SSID']=='Cafe, "Mochi"' and rows[0]['Type']=='WIFI'
assert rows[1]['Type']=='BLE' and rows[1]['Channel']=='0'
assert float(rows[0]['AccuracyMeters'])==6.0
print('WiGLE CSV round-trip passed (11 columns, quotes, BLE, UTC, accuracy units).')

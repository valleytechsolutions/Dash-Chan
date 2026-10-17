"""Create a local flash bundle from a successful PlatformIO build."""
from pathlib import Path
import hashlib, os, shutil, zipfile
root=Path(__file__).resolve().parents[1]
build=root/'.pio/build/stackchan'
dest=root/'build/Dash-Chan-0.1.0-alpha'
dest.mkdir(parents=True,exist_ok=True)
for name in ['firmware.bin','bootloader.bin','partitions.bin']:
    shutil.copy2(build/name,dest/name)
packages=Path(os.environ.get('PLATFORMIO_PACKAGES_DIR',Path.home()/'.platformio/packages'))
shutil.copy2(packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin',dest/'boot_app0.bin')
(dest/'INSTALL.txt').write_text('Dash-Chan 0.1.0 alpha for official M5Stack StackChan K151 only.\nBack up the full device flash before installing.\nUse split images to leave the NVS/calibration sectors untouched; never erase flash first.\nExample for esptool 5:\npython -m esptool --chip esp32s3 --port YOUR_PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin\nApp-only updates require an existing Dash-Chan partition layout.\nGPS model/wiring and validation: https://github.com/valleytechsolutions/Dash-Chan\nNo factory backup, personal observations, or credentials are in this bundle.\n')
checksums={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in dest.glob('*.bin')}
(dest/'SHA256SUMS.txt').write_text(''.join(f'{v}  {k}\n' for k,v in checksums.items()))
archive=dest.parent/(dest.name+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in dest.iterdir():z.write(p,p.name)
print(archive)

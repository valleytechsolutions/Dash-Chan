"""Create a local flash bundle from a successful PlatformIO build."""
from pathlib import Path
import hashlib, os, shutil, zipfile, re, json
root=Path(__file__).resolve().parents[1]
build=root/'.pio/build/stackchan'
version=re.search(r'Version\s*=\s*"([^"]+)"',(root/'include/config.h').read_text()).group(1)
dest=root/f'build/Dash-Chan-{version}'
dest.mkdir(parents=True,exist_ok=True)
for name in ['firmware.bin','bootloader.bin','partitions.bin']:
    shutil.copy2(build/name,dest/name)
shutil.copy2(root/'LICENSE', dest/'LICENSE')
shutil.copytree(root/'docs/licenses', dest/'licenses', dirs_exist_ok=True)
shutil.copy2(root/'lib/opendroneid/LICENSE', dest/'licenses/OPENDRONEID-APACHE-2.0.txt')
shutil.copy2(root/'lib/DashChanSD/LICENSE', dest/'licenses/SD-APACHE-2.0.txt')
dependencies=root/'.pio/libdeps/stackchan'
for library in ['StackChan-BSP','M5Unified','M5GFX','M5HAL','M5UnitUnified','M5Utility','NimBLE-Arduino']:
    for notice in ['LICENSE','NOTICE']:
        source=dependencies/library/notice
        if source.is_file():
            shutil.copy2(source,dest/'licenses'/f'{library}-{notice}.txt')
gps_header=(dependencies/'TinyGPSPlus/src/TinyGPS++.h').read_text(encoding='utf-8')
(dest/'licenses/TinyGPSPlus-NOTICE.txt').write_text(gps_header.split('*/',1)[0]+'*/\n',encoding='utf-8')
(dest/'SOURCE.txt').write_text(f'Corresponding source and build instructions: https://github.com/valleytechsolutions/Dash-Chan/tree/v{version}\nDependency versions and credits: https://github.com/valleytechsolutions/Dash-Chan/blob/v{version}/docs/SOURCES.md\nDash-Chan is GPL-3.0-or-later; imported components retain their notices.\n')
packages=Path(os.environ.get('PLATFORMIO_PACKAGES_DIR',Path.home()/'.platformio/packages'))
shutil.copy2(packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin',dest/'boot_app0.bin')
(dest/'INSTALL.txt').write_text(f'Dash-Chan {version} for official M5Stack StackChan K151 only.\nBack up the full device flash before installing.\nUse split images to leave the NVS/calibration sectors untouched; never erase flash first.\nExample for esptool 5:\npython -m esptool --chip esp32s3 --port YOUR_PORT write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin\nApp-only updates require an existing Dash-Chan partition layout.\nWeb flasher: https://valleytechsolutions.github.io/Dash-Chan/\nGuide: https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/FLASHING.md\nNo factory backup, personal observations, or credentials are in this bundle.\n')
checksums={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in dest.glob('*.bin')}
(dest/'SHA256SUMS.txt').write_text(''.join(f'{v}  {k}\n' for k,v in checksums.items()))
archive=dest.parent/(dest.name+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in dest.rglob('*'):
        if p.is_file():z.write(p,p.relative_to(dest))
print(archive)
web=root/'site/firmware';web.mkdir(parents=True,exist_ok=True)
parts=[('bootloader.bin',0,0x8000),('partitions.bin',0x8000,0x9000),('boot_app0.bin',0xe000,0x10000),('firmware.bin',0x10000,0x510000)]
manifest={'schema':1,'version':version,'chip':'ESP32-S3','flashSize':16777216,'files':[]}
for name,offset,end in parts:
    data=(dest/name).read_bytes()
    assert len(data)>0 and len(data)%4==0 and ((offset+len(data)+4095)//4096)*4096<=end
    if name=='bootloader.bin':assert data[0]==0xe9 and data[2]==2, 'K151 bootloader must be DIO'
    shutil.copy2(dest/name,web/name)
    manifest['files'].append({'name':name,'path':f'firmware/{name}','offset':offset,'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'md5':hashlib.md5(data).hexdigest()})
(web/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
shutil.copy2(dest/'SHA256SUMS.txt',web/'SHA256SUMS.txt')
shutil.copy2(archive,web/'Dash-Chan.zip')

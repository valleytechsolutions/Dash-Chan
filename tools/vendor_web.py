"""Refresh pinned self-hosted browser dependencies and their license notices."""
from pathlib import Path
import urllib.request,json,tarfile,io,hashlib,base64
root=Path(__file__).resolve().parents[1];dest=root/'site/vendor';dest.mkdir(parents=True,exist_ok=True)
ledger=[]
for package,version in [('esptool-js','0.7.0'),('pako','2.1.0'),('atob-lite','2.0.0')]:
    meta=json.load(urllib.request.urlopen(f'https://registry.npmjs.org/{package}/{version}'))
    raw=urllib.request.urlopen(meta['dist']['tarball']).read()
    integrity=meta['dist']['integrity'];algorithm,expected=integrity.split('-',1)
    assert base64.b64encode(hashlib.new(algorithm,raw).digest()).decode()==expected
    archive=tarfile.open(fileobj=io.BytesIO(raw),mode='r:gz')
    notices=[]
    for member in archive.getmembers():
        name=Path(member.name).name
        if member.isfile() and name.upper().startswith(('LICENSE','COPYING','NOTICE')):
            filename=f'{package}-{name}.txt';(dest/filename).write_bytes(archive.extractfile(member).read());notices.append(filename)
    assert notices,f'Missing license for {package}'
    if package=='esptool-js':(dest/'esptool-0.7.0.js').write_bytes(archive.extractfile('package/bundle.js').read())
    ledger.append({'name':package,'version':version,'tarball':meta['dist']['tarball'],'integrity':integrity,'license':meta['license'],'notices':notices})
(dest/'dependencies.json').write_text(json.dumps(ledger,indent=2)+'\n')
(dest/'NOTICE.txt').write_text('Dash-Chan browser flasher uses the unmodified esptool-js 0.7.0 browser bundle.\nEspressif esptool-js: Apache-2.0, https://github.com/espressif/esptool-js\nIts bundled runtime dependencies are pako 2.1.0 (MIT AND Zlib) and atob-lite 2.0.0 (MIT).\nOriginal license files are in this directory; dependencies.json records exact tarballs and integrity values.\nNo runtime CDN, analytics, or external font dependency is used.\n')
print('Pinned web dependencies and licenses verified.')

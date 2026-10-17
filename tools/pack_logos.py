"""Encode the supplied, unmodified brand artwork at LCD resolution."""
from pathlib import Path
from PIL import Image, ImageOps
import io
root=Path(__file__).resolve().parents[1]
out=['// Supplied BWM / VTS marks. Brand rights retained by their owners.', '#pragma once', '#include <stdint.h>']
for symbol,filename in [('bootBwm','bwm.png'),('bootVts','vts.png')]:
    source=Image.open(root/'assets/brands'/filename).convert('RGBA')
    scaled=ImageOps.contain(source,(122,138),Image.Resampling.LANCZOS)
    tile=Image.new('RGBA',(122,138),'white')
    tile.alpha_composite(scaled,((122-scaled.width)//2,(138-scaled.height)//2))
    buf=io.BytesIO();tile.convert('RGB').save(buf,format='PNG',optimize=True)
    data=buf.getvalue();out.append(f'static const uint8_t {symbol}[] = {{')
    for i in range(0,len(data),24):out.append('  '+','.join(map(str,data[i:i+24]))+',')
    out.append('};')
(root/'include/boot_logos.h').write_text('\n'.join(out)+'\n')

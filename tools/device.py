"""USB diagnostics and framebuffer capture. Does not reset the board."""
import argparse, time
from pathlib import Path
import serial
p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('command',nargs='?',default='STATUS')
p.add_argument('--output',default='build/device-screen.png')
a=p.parse_args()
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.2;s.write_timeout=2;s.dtr=True;s.rts=False
with s:
    s.reset_input_buffer();s.write((a.command+'\n').encode())
    if a.command=='SCREEN':
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            if s.readline().strip()==b'FRAME 320 240 RGB888':break
        else:raise RuntimeError('No framebuffer header')
        data=bytearray();deadline=time.monotonic()+15
        while len(data)<320*240*3 and time.monotonic()<deadline:data.extend(s.read(320*240*3-len(data)))
        assert len(data)==320*240*3,'Truncated frame'
        from PIL import Image
        Path(a.output).parent.mkdir(parents=True,exist_ok=True)
        Image.frombytes('RGB',(320,240),bytes(data)).save(a.output)
        print('Saved',a.output)
    else:
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            line=s.readline().decode('utf-8','replace').strip()
            if line:print(line)

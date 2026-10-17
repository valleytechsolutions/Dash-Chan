// Dash-Chan installer policy, separate from the page so it can be regression-tested.
import {parseAvatarLine} from './profile.mjs';
export {avatarLine} from './profile.mjs';
export function validateManifest(m) {
  if(m?.schema!==1||m.chip!=='ESP32-S3'||m.flashSize!==16777216||!/^\d+\.\d+\.\d+[-\w.]*$/.test(m.version)||m.files?.length!==4)throw Error('Unsupported or incomplete firmware manifest.');
  const ranges={'bootloader.bin':[0,0x8000],'partitions.bin':[0x8000,0x9000],'boot_app0.bin':[0xe000,0x10000],'firmware.bin':[0x10000,0x510000]};
  const seen=new Set();
  for(const f of m.files){const r=ranges[f.name];if(!r||seen.has(f.name)||f.path!==`firmware/${f.name}`||f.offset!==r[0]||!Number.isInteger(f.size)||f.size<=0||f.size%4||Math.ceil((f.offset+f.size)/4096)*4096>r[1]||!/^[a-f0-9]{64}$/.test(f.sha256)||!/^[a-f0-9]{32}$/.test(f.md5))throw Error('Unsafe image layout or invalid checksum metadata.');seen.add(f.name);}
  return m;
}
export async function sha256(bytes){return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),v=>v.toString(16).padStart(2,'0')).join('');}
export async function loadManifest(fetcher=fetch){const r=await fetcher(new URL('firmware/manifest.json',import.meta.url),{cache:'no-store',signal:AbortSignal.timeout(20000)});if(!r.ok)throw Error(`Release information unavailable (HTTP ${r.status}).`);return validateManifest(await r.json());}
export async function loadImages(m,fetcher=fetch,onStatus=()=>{}){
  validateManifest(m);const images=[];
  for(const f of m.files){onStatus(`Checking ${f.name}…`);const r=await fetcher(new URL(f.path,import.meta.url),{cache:'no-store',signal:AbortSignal.timeout(60000)});if(!r.ok)throw Error(`Could not download ${f.name} (HTTP ${r.status}).`);const data=new Uint8Array(await r.arrayBuffer());if(data.length!==f.size||await sha256(data)!==f.sha256)throw Error(`Checksum failed for ${f.name}. Nothing has been written. Reload and try again.`);images.push({data,address:f.offset});}
  return images;
}
export async function checkBoard(loader){if(loader.chip?.CHIP_NAME!=='ESP32-S3')throw Error('Wrong chip. Dash-Chan requires the official ESP32-S3 StackChan K151.');if(await loader.detectFlashSize()!=='16MB')throw Error('Flash size mismatch. Expected 16 MB. No firmware was written.');}
export async function restartBoard(loader){
  // esptool-js 0.7.0 HardReset omits the RTS assertion. Explicitly pulse EN
  // with BOOT released; verified on K151's USB Serial/JTAG interface.
  await loader.after('custom_reset',false,'D0|R1|W100|R0|W100');
}
export async function installImages(loader,m,images,onStatus=()=>{},onProgress=()=>{}){
  validateManifest(m);await checkBoard(loader);
  if(images.length!==m.files.length)throw Error('Missing firmware image.');
  for(let i=0;i<images.length;i++){const f=m.files[i];if(images[i].address!==f.offset||images[i].data.length!==f.size||await sha256(images[i].data)!==f.sha256)throw Error('Image changed after download. Nothing written.');}
  onStatus('Reading calibration/settings sectors…');const nvs=await loader.readFlash(0x9000,0x5000);if(nvs.length!==0x5000)throw Error('Could not read calibration sectors. Nothing written.');const nvsHash=await sha256(nvs);
  onStatus('Writing firmware. Keep USB connected…');
  // Packaged bootloader is DIO / 80 MHz / 16 MB. Keep its verified bytes intact.
  await loader.writeFlash({fileArray:images,flashMode:'keep',flashFreq:'keep',flashSize:'16MB',eraseAll:false,compress:true,reportProgress:(i,w,t)=>onProgress(5+80*(i+(t?w/t:0))/images.length)});
  onStatus('Verifying written flash…');
  for(let i=0;i<m.files.length;i++){const f=m.files[i];const hash=String(await loader.flashMd5sum(f.offset,f.size)).toLowerCase();if(hash!==f.md5)throw Error(`Written checksum failed for ${f.name}. Reconnect in boot mode and reinstall.`);onProgress(85+10*(i+1)/m.files.length);}
  const after=await loader.readFlash(0x9000,0x5000);
  if(after.length!==0x5000||await sha256(after)!==nvsHash)throw Error('Calibration/settings verification failed. Keep your backup and stop here.');
  onStatus('Firmware verified; calibration/settings preserved. Restarting…');
  await restartBoard(loader);onProgress(100);
}
export async function sendAvatar(port,line,onStatus=()=>{}){
  parseAvatarLine(line);
  if(/[\r\n]/.test(line))throw Error('Send one avatar profile at a time.');
  // Application protocol: does not reset the robot or enter its bootloader.
  let reader,writer,timer;let received='';
  try{
    await port.open({baudRate:115200});await port.setSignals({dataTerminalReady:true,requestToSend:false});
    reader=port.readable.getReader();writer=port.writable.getWriter();
    const encoder=new TextEncoder(),decoder=new TextDecoder();
    async function command(command,pattern){
      received='';await writer.write(encoder.encode(command+'\n'));
      const deadline=Date.now()+8000;
      while(Date.now()<deadline){const remaining=deadline-Date.now();const chunk=await Promise.race([reader.read(),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('No USB reply. Use Dash-Chan 0.2+ in normal running mode.')),remaining);})]);clearTimeout(timer);if(chunk.done)throw Error('USB connection closed.');received+=decoder.decode(chunk.value,{stream:true});if(received.length>16384)received=received.slice(-8192);const found=received.match(pattern);if(found)return found;}
      throw Error('USB response timed out.');
    }
    onStatus('Checking the running Dash-Chan firmware…');
    const match=await command('STATUS',/STATUS (\{[^\r\n]+\})/);const status=JSON.parse(match[1]);
    const version=/^(\d+)\.(\d+)\.(\d+)(?:[-+][\w.-]+)?$/.exec(status.version);
    if(!version||!(Number(version[1])>0||Number(version[2])>=2)||status.board!==27)throw Error('This uploader needs Dash-Chan 0.2+ on StackChan K151.');
    if(line.startsWith('DCAV2 ')&&status.avatar_schema!==2)throw Error('Install Dash-Chan 0.3+ first to use the new eyes and layered outfits. Your current avatar has not changed.');
    if(parseAvatarLine(line).base>=3 && !(status.avatar_bases>=5))throw Error('Install Dash-Chan 0.4+ first for Wisp and Ember. Your current avatar has not changed.');
    if(status.recording)throw Error('Tap STOP & SAVE before changing your avatar.');
    onStatus('Sending your new buddy…');const response=await command('AVATAR SET '+line,/AVATAR (OK|ERROR[^\r\n]*)/);if(response[1]!=='OK')throw Error(response[1]);
    return true;
  }finally{clearTimeout(timer);if(reader){await reader.cancel().catch(()=>{});reader.releaseLock();}if(writer)writer.releaseLock();await port.close().catch(()=>{});}
}

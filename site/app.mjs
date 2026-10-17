import {loadManifest,loadImages,checkBoard,installImages,restartBoard,sendAvatar,sha256} from './flasher.mjs';
import {initStudio} from './studio.mjs';
const $=id=>document.getElementById(id);
let manifest,loader,transport,busy=false,uploading=false;
const supported=!!navigator.serial&&window.isSecureContext;
function log(message){const el=$('flash-log');el.textContent=(el.textContent+String(message)+'\n').slice(-16000);el.scrollTop=el.scrollHeight;}
function status(message,error=false){$('flash-status').textContent=message;$('flash-status').className=error?'error':'';}
function controls(){const linked=!!loader;const locked=busy||uploading;$('connect').disabled=!supported||!manifest||linked||locked||!$('hardware-confirm').checked;$('disconnect').disabled=!linked||locked;$('backup').disabled=!linked||locked;$('flash').disabled=!linked||locked;$('hardware-confirm').disabled=linked||locked;$('upload-avatar').disabled=!supported||linked||locked;$('connection-badge').textContent=linked?'CONNECTED · ESP32-S3':'NOT CONNECTED';}
function download(bytes,name,type='application/octet-stream'){const url=URL.createObjectURL(new Blob([bytes],{type}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);}
async function closeConnection(){const t=transport;loader=undefined;transport=undefined;if(t)await t.disconnect().catch(e=>log(`Port cleanup: ${e.message}`));controls();}
$('hardware-confirm').addEventListener('change',controls);
if(!supported){$('browser-help').textContent='USB tools require desktop Chrome or Edge on HTTPS. Download the ZIP for manual flashing in other browsers.';$('browser-help').classList.add('error');}
loadManifest().then(m=>{manifest=m;$('version').textContent=`Dash-Chan ${m.version}`;status('Ready. Confirm your K151, then connect.');controls();}).catch(e=>{status(e.message+' Download a release from GitHub if this persists.',true);controls();});
$('connect').addEventListener('click',async()=>{
  if(busy||uploading)return;busy=true;controls();
  try{
    const port=await navigator.serial.requestPort({filters:[{usbVendorId:0x303a}]});
    status('Connecting to the bootloader…');
    const {ESPLoader,Transport}=await import('./vendor/esptool-0.7.0.js');
    transport=new Transport(port,false);loader=new ESPLoader({transport,baudrate:460800,terminal:{clean(){},write:log,writeLine:log},debugLogging:false});
    await loader.main();await checkBoard(loader);status('Connected to ESP32-S3 with 16 MB flash. Download a backup or install.');
  }catch(e){log(e.message);status(e.name==='NotFoundError'?'Port selection canceled.':`${e.message} Close serial monitors; hold the bottom RST button about 3 seconds, release at the green LED, and reconnect.`,e.name!=='NotFoundError');await closeConnection();}
  finally{busy=false;controls();}
});
$('disconnect').addEventListener('click',async()=>{if(busy)return;busy=true;controls();try{await restartBoard(loader);}catch(e){log(e.message);}await closeConnection();busy=false;controls();status('Disconnected. Your robot is restarting.');});
$('backup').addEventListener('click',async()=>{
  if(!loader||busy)return;busy=true;controls();$('progress').value=0;status('Reading a full 16 MB backup. This can take several minutes…');
  try{const bytes=await loader.readFlash(0,16777216,(_,read,total)=>{$('progress').value=100*read/total;});if(bytes.length!==16777216)throw Error('Incomplete backup. No download created.');const digest=await sha256(bytes);const filename=`stackchan-backup-${new Date().toISOString().slice(0,10)}.bin`;download(bytes,filename);download(`${digest}  ${filename}\n`,filename+'.sha256','text/plain');$('progress').value=100;status('Backup downloaded. Keep it private and safe. You can now install Dash-Chan.');}
  catch(e){status(e.message,true);await closeConnection();}finally{busy=false;controls();}
});
$('flash').addEventListener('click',async()=>{
  if(!loader||busy)return;busy=true;controls();$('progress').value=0;
  try{const images=await loadImages(manifest,fetch,status);await installImages(loader,manifest,images,status,v=>{$('progress').value=v;});status('Installed and verified! Dash-Chan is restarting. Look for your new buddy.');$('flash-status').className='success';}
  catch(e){log(e.message);status(`${e.message} Keep the data cable connected; reconnect in boot mode to retry.`,true);}
  finally{await closeConnection();busy=false;controls();}
});
window.addEventListener('beforeunload',e=>{if(busy||uploading){e.preventDefault();e.returnValue='';}});
initStudio({
  upload:async(line,onStatus)=>{
    if(busy||loader||uploading)throw Error('Disconnect the installer before sending your buddy.');
    uploading=true;controls();
    try{const port=await navigator.serial.requestPort({filters:[{usbVendorId:0x303a}]});await sendAvatar(port,line,onStatus);}
    finally{uploading=false;controls();}
  }
});
controls();

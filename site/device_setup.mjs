import {validateUploadConfig} from './uploads.mjs';
export async function sendUploadConfig(port,config,onStatus=()=>{}) {
  const data=new TextEncoder().encode(JSON.stringify(validateUploadConfig(config)));
  let reader,writer,timer;
  try {
    await port.open({baudRate:115200});await port.setSignals({dataTerminalReady:true,requestToSend:false});
    reader=port.readable.getReader();writer=port.writable.getWriter();
    const enc=new TextEncoder(),dec=new TextDecoder();
    async function command(line,pattern) {
      await writer.write(enc.encode(line+'\n'));let received='';const end=Date.now()+10000;
      // USB CDC can miss the first request immediately after DTR is asserted.
      // Retry only the read-only handshake, never a setup mutation.
      const retry=line==='STATUS'?setInterval(()=>{writer.write(enc.encode('STATUS\n')).catch(()=>{});},750):null;
      try {
      while(Date.now()<end) {
        const chunk=await Promise.race([reader.read(),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('No reply from Dash-Chan. Use firmware 0.5+ in normal running mode.')),Math.max(1,end-Date.now()));})]);
        clearTimeout(timer);if(chunk.done)throw Error('USB disconnected before setup completed.');
        received+=dec.decode(chunk.value,{stream:true});if(received.length>16384)received=received.slice(-8192);
        const match=received.match(pattern);if(match)return match;
      }
      throw Error('Setup transfer timed out.');
      }finally{clearInterval(retry);}
    }
    onStatus('Checking the running firmware…');
    const info=JSON.parse((await command('STATUS',/STATUS (\{[^\r\n]+\})/))[1]);
    if(info.board!==27||info.setup_schema!==1)throw Error('Install Dash-Chan 0.5+ on StackChan K151 first.');
    if(info.recording||info.upload_busy||!info.sd)throw Error('Stop and save, finish any upload, and insert an SD card first.');
    const send=async line=>{const reply=await command(line,/SETUP (OK[^\r\n]*|ERROR[^\r\n]*)/);if(!reply[1].startsWith('OK'))throw Error(reply[1]);return reply[1];};
    onStatus('Saving upload settings to your SD card…');await send('SETUP BEGIN');
    for(let i=0;i<data.length;i+=40)await send('SETUP DATA '+Array.from(data.slice(i,i+40),v=>v.toString(16).padStart(2,'0')).join(''));
    if(await send('SETUP END')!=='OK SAVED')throw Error('Device did not confirm saving setup.');
    return true;
  } finally {
    data.fill(0);clearTimeout(timer);
    if(reader){await reader.cancel().catch(()=>{});reader.releaseLock();}if(writer)writer.releaseLock();await port.close().catch(()=>{});
  }
}

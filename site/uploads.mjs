// SPDX-License-Identifier: GPL-3.0-or-later
const utf8 = new TextEncoder();
const stringFields = {ssid:32,password:64,wigleName:128,wigleToken:128,wdgKey:64};
const boolFields = ['wigle','wdg','automatic','donate'];
export function validateUploadConfig(input) {
  if(!input || input.schema!==1) throw Error('Use a Dash-Chan upload setup, schema 1.');
  const c={schema:1};
  for(const [key,max] of Object.entries(stringFields)) {
    const v=input[key];
    if(typeof v!=='string'||utf8.encode(v).length>max||/[\x00-\x1f\x7f]/.test(v)) throw Error(`Invalid ${key}: maximum ${max} bytes, no control characters.`);
    c[key]=v;
  }
  for(const key of boolFields) {if(typeof input[key]!=='boolean') throw Error(`Invalid ${key} setting.`);c[key]=input[key];}
  if(!c.ssid) throw Error('Enter your Wi-Fi network name.');
  if(!c.wigle&&!c.wdg) throw Error('Choose at least one upload destination.');
  if(c.wigle&&(!/^[!-9;-~]{1,128}$/.test(c.wigleName)||!/^[!-~]{1,128}$/.test(c.wigleToken))) throw Error('Enter the WiGLE API name and token (not your account password).');
  if(c.wdg&&!/^[a-f\d]{64}$/i.test(c.wdgKey)) throw Error('The WDGWars API key must be 64 hexadecimal characters.');
  return c;
}
export function parseCsvLine(line) {
  const fields=[];let field='',quoted=false,closed=false;
  for(let i=0;i<line.length;i++) {
    const c=line[i];
    if(/[\r\n\0]/.test(c)) throw Error('Multiline or binary CSV fields are not supported.');
    if(quoted) {
      if(c==='"') {if(line[i+1]==='"'){field+='"';i++;}else{quoted=false;closed=true;}}
      else field+=c;
    } else if(c===','){fields.push(field);field='';closed=false;}
    else if(c==='"'&&!field&&!closed) quoted=true;
    else if(closed||c==='"') throw Error('Invalid CSV quoting.');
    else field+=c;
    if(field.length>512||fields.length>14) throw Error('CSV field or column limit exceeded.');
  }
  if(quoted) throw Error('Unclosed CSV quote.');
  fields.push(field);return fields;
}
const columns14='MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type';
const columns16='MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type';
const quote=s=>'"'+String(s).replaceAll('"','""')+'"';
export function parseDrive(text) {
  if(utf8.encode(text).length>40*1024*1024) throw Error('Choose a drive smaller than 40 MiB.');
  const lines=text.replace(/^\uFEFF/,'').trimEnd().split(/\r?\n/);
  const modern=lines[0]?.startsWith('WigleWifi-1.6,');
  if(!modern&&!lines[0]?.startsWith('WigleWifi-1.4,')) throw Error('Choose a WiGLE 1.4 or 1.6 CSV drive.');
  const columns=modern?columns16:columns14;
  if(lines[1]!==columns) throw Error('Unexpected CSV columns. Export an original Dash-Chan drive.');
  if(lines.length<3||lines.length>500002) throw Error('Drive must contain 1–500,000 observations.');
  const names=columns.split(','),preview=[],converted=[];
  let wifi=0,ble=0,minLat=90,maxLat=-90,minLon=180,maxLon=-180;
  for(let i=2;i<lines.length;i++) {
    const f=parseCsvLine(lines[i]);
    if(f.length!==names.length) throw Error(`Wrong column count on line ${i+1}.`);
    const row=Object.fromEntries(names.map((name,j)=>[name,f[j]]));
    if(!/^(?:[\da-f]{2}:){5}[\da-f]{2}$/i.test(row.MAC)||!['BLE','WIFI'].includes(row.Type)) throw Error(`Invalid address or type on line ${i+1}.`);
    const lat=Number(row.CurrentLatitude),lon=Number(row.CurrentLongitude);
    if(!row.CurrentLatitude.trim()||!row.CurrentLongitude.trim()||!Number.isFinite(lat)||!Number.isFinite(lon)||Math.abs(lat)>90||Math.abs(lon)>180) throw Error(`Invalid coordinates on line ${i+1}.`);
    if(!/^\d+$/.test(row.Channel)) throw Error(`Invalid channel on line ${i+1}.`);
    if(!modern&&Number(row.Channel)>14) throw Error('Legacy converter supports 2.4 GHz Dash-Chan captures.');
    if(!/^-?\d+$/.test(row.RSSI)||!/^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d$/.test(row.FirstSeen)) throw Error(`Invalid signal or UTC timestamp on line ${i+1}.`);
    for(const key of ['AltitudeMeters','AccuracyMeters']) if(!row[key]||!Number.isFinite(Number(row[key]))||(key==='AccuracyMeters'&&Number(row[key])<=0)) throw Error(`Invalid ${key} on line ${i+1}.`);
    minLat=Math.min(minLat,lat);maxLat=Math.max(maxLat,lat);minLon=Math.min(minLon,lon);maxLon=Math.max(maxLon,lon);
    row.Type==='WIFI'?wifi++:ble++;
    if(preview.length<5) preview.push(row);
    if(!modern) {
      const ch=Number(row.Channel),freq=row.Type==='BLE'||ch===0?0:ch===14?2484:2407+ch*5;
      converted.push([...f.slice(0,5),freq,...f.slice(5,10),'',0,f[10]].map(quote).join(','));
    }
  }
  return {wifi,ble,rows:wifi+ble,preview,bounds:{minLat,maxLat,minLon,maxLon},
    wdg:modern?text:'WigleWifi-1.6,appRelease=0.5.0-beta,model=StackChan,device=ESP32-S3,board=K151,brand=M5Stack\n'+columns16+'\n'+converted.join('\n')+'\n'};
}
export async function sendWigle(file,name,token,donate,signal,fetcher=fetch) {
  if(!/^[!-9;-~]{1,128}$/.test(name)||!/^[!-~]{1,128}$/.test(token)) throw Error('Enter a valid WiGLE API name and token.');
  const body=new FormData();body.append('file',file,file.name||'dashchan.csv');body.append('donate',donate?'on':'false');
  const response=await fetcher('https://api.wigle.net/api/v2/file/upload',{method:'POST',headers:{Authorization:'Basic '+btoa(name+':'+token)},body,signal,redirect:'error',credentials:'omit'});
  if(!response.ok) throw Error(`WiGLE returned HTTP ${response.status}. Check your account before retrying.`);
  const result=await response.json();
  if(result.success!==true) throw Error('WiGLE did not confirm acceptance. Check your account before retrying.');
  return result;
}
export function initUploads({upload}={}) {
  const $=id=>document.getElementById(id),form=$('upload-config-form');
  let drive,parsed,busy=false,controller;
  function download(data,name,type){const url=URL.createObjectURL(new Blob([data],{type}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);}
  function controls(){
    $('send-drive').disabled=busy||!parsed||!$('share-confirm').checked;
    $('cancel-drive').disabled=!busy;$('export-wdg').disabled=busy||!parsed;
    for(const id of ['drive-file','browser-wigle-name','browser-wigle-token','browser-donate','share-confirm']) $(id).disabled=busy;
  }
  $('drive-file').addEventListener('change',async e=>{
    drive=undefined;parsed=undefined;$('share-confirm').checked=false;$('drive-preview').hidden=true;$('upload-status').textContent='';controls();
    const file=e.target.files[0];if(!file)return;
    try{
      if(file.size>40*1024*1024) throw Error('Choose a drive smaller than 40 MiB.');
      const data=parseDrive(await file.text());parsed=data;drive=file;
      const b=data.bounds;
      $('drive-summary').textContent=`${data.rows.toLocaleString()} observations: ${data.wifi.toLocaleString()} Wi-Fi, ${data.ble.toLocaleString()} BLE. Latitude ${b.minLat.toFixed(4)} to ${b.maxLat.toFixed(4)}, longitude ${b.minLon.toFixed(4)} to ${b.maxLon.toFixed(4)}. These are observations, not unique-network totals.`;
      const body=$('drive-preview').querySelector('tbody');body.replaceChildren();
      for(const row of data.preview){const tr=document.createElement('tr');for(const value of [row.SSID||'(hidden)',row.Type,`${Number(row.CurrentLatitude).toFixed(5)}, ${Number(row.CurrentLongitude).toFixed(5)}`]){const td=document.createElement('td');td.textContent=value;tr.append(td);}body.append(tr);}
      $('drive-preview').hidden=false;
    }catch(e){$('drive-summary').textContent=e.message;}
    controls();
  });
  $('share-confirm').addEventListener('change',controls);
  $('export-wdg').addEventListener('click',()=>{if(parsed)download(parsed.wdg,'dashchan-wdgwars.csv','text/csv');});
  $('send-drive').addEventListener('click',async()=>{
    if(busy||!parsed||!drive||!$('share-confirm').checked)return;
    busy=true;controller=new AbortController();controls();$('upload-status').textContent='Sending directly to WiGLE…';
    try{await sendWigle(drive,$('browser-wigle-name').value.trim(),$('browser-wigle-token').value.trim(),$('browser-donate').checked,controller.signal);$('upload-status').textContent='WiGLE accepted your file. Processing may continue in your account.';$('share-confirm').checked=false;}
    catch(e){$('upload-status').textContent=e.name==='AbortError'?'Canceled. The service may have received the file; check your account before retrying.':e.message+' If the connection failed, the result may be uncertain. No automatic retry was made.';}
    finally{busy=false;controls();}
  });
  $('cancel-drive').addEventListener('click',()=>controller?.abort());
  window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
  form.addEventListener('submit',e=>{
    e.preventDefault();
    try{const c={schema:1};for(const key of Object.keys(stringFields))c[key]=form.elements[key].value;for(const key of boolFields)c[key]=form.elements[key].checked;download(JSON.stringify(validateUploadConfig(c),null,2)+'\n','upload.json','application/json');$('config-status').textContent='Setup downloaded. Keep it private and copy it to /dashchan/ on your SD card.';}
    catch(e){$('config-status').textContent=e.message;}
  });
  $('send-setup').addEventListener('click',async()=>{
    if(!form.reportValidity())return;
    try {
      const c={schema:1};for(const key of Object.keys(stringFields))c[key]=form.elements[key].value;for(const key of boolFields)c[key]=form.elements[key].checked;
      await upload(validateUploadConfig(c),message=>$('config-status').textContent=message);
      $('config-status').textContent='Setup saved and verified on StackChan. No upload has started. Open Menu → Upload drives to choose a drive.';
    }catch(e){$('config-status').textContent=e.name==='NotFoundError'?'Port selection canceled.':e.message;}
  });
  $('import-upload-config').addEventListener('change',async e=>{
    try{const f=e.target.files[0];if(!f)return;if(f.size>2048)throw Error('Setup file is too large.');const c=validateUploadConfig(JSON.parse(await f.text()));for(const key of Object.keys(stringFields))form.elements[key].value=c[key];for(const key of boolFields)form.elements[key].checked=c[key];$('config-status').textContent='Setup loaded for editing. No connection or upload has started.';}
    catch(e){$('config-status').textContent=e.message;}finally{e.target.value='';}
  });
}

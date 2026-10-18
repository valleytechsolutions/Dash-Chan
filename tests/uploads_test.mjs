import test from 'node:test';
import assert from 'node:assert/strict';
import {validateUploadConfig,parseDrive,parseCsvLine,sendWigle} from '../site/uploads.mjs';
import {sendUploadConfig} from '../site/device_setup.mjs';
const setup={schema:1,ssid:'My network',password:'password123',wigleName:'api-name',wigleToken:'api-token',wdgKey:'a'.repeat(64),wigle:true,wdg:true,automatic:false,donate:false};
const header='WigleWifi-1.4,appRelease=test\nMAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n';
const row='02:00:00:00:00:01,"Cafe, ""Mochi""",[WPA2-PSK][ESS],2026-10-17 12:30:00,6,-42,40.0000000,-75.0000000,12,5.0,WIFI\n';
test('setup has explicit destinations and opt-in automatic upload; rejects unsafe and oversized values',()=>{
  assert.deepEqual(validateUploadConfig(setup),setup);
  for(const change of [{schema:2},{automatic:'false'},{wigle:false,wdg:false},{ssid:''},{ssid:'é'.repeat(17)},{password:'a\r\nb'},{wdgKey:'bad'},{wigleName:'username:password'},{wigleToken:''}]) assert.throws(()=>validateUploadConfig({...setup,...change}));
  assert.equal(validateUploadConfig({...setup,password:''}).password,'');
});
test('CSV preview preserves quotes, classifies observations and converts 1.4 to 1.6 accurately',()=>{
  const p=parseDrive(header+row+'02:00:00:00:00:02,"tracker",[BLE],2026-10-17 12:30:01,0,-70,40.1,-75.2,13,6.0,BLE\n');
  assert.equal(p.rows,2);assert.equal(p.wifi,1);assert.equal(p.ble,1);assert.equal(p.preview[0].SSID,'Cafe, "Mochi"');
  assert.deepEqual(p.bounds,{minLat:40,maxLat:40.1,minLon:-75.2,maxLon:-75});
  const lines=p.wdg.trim().split('\n');assert.equal(parseCsvLine(lines[2]).length,14);assert.equal(parseCsvLine(lines[2])[5],'2437');assert.equal(parseCsvLine(lines[3])[5],'0');assert.equal(parseCsvLine(lines[2])[11],'');assert.equal(parseCsvLine(lines[2])[12],'0');
  assert.equal(parseDrive(p.wdg).wdg,p.wdg);
});
test('CSV rejects malformed quotes, absent observations, nonfinite/out-of-bounds coordinates and bad columns',()=>{
  for(const text of [header,header+row.replace('40.0000000','NaN'),header+row.replace('40.0000000','91'),header+row.replace('40.0000000',''),header+row.replace('Cafe, ""Mochi""','Cafe"Mochi'),header+row.replace(',6,-42',',99,-42'),header+row.replace(',5.0,',',0,'),header.replace('SSID','Wrong')+row]) assert.throws(()=>parseDrive(text));
  assert.throws(()=>parseCsvLine('"unclosed'));assert.throws(()=>parseCsvLine('"closed"junk'));
});
test('upload sends only to WiGLE, includes explicit donate choice, prohibits redirects, accepts only confirmed success',async()=>{
  const file=new File([header+row],'test.csv',{type:'text/csv'});
  let count=0;
  const fetcher=async(url,opts)=>{count++;assert.equal(url,'https://api.wigle.net/api/v2/file/upload');assert.equal(opts.redirect,'error');assert.equal(opts.credentials,'omit');assert.equal(opts.headers.Authorization,'Basic '+btoa('api:token'));assert.equal(opts.body.get('donate'),'false');assert.equal(await opts.body.get('file').text(),header+row);return new Response(JSON.stringify({success:true}));};
  await sendWigle(file,'api','token',false,undefined,fetcher);assert.equal(count,1);
  for(const response of [new Response('{}'),new Response('{"success":false}'),new Response('{"success":true}',{status:401})]) await assert.rejects(sendWigle(file,'api','token',false,undefined,async()=>response));
  await assert.rejects(sendWigle(file,'api','token',false,undefined,async()=>{throw Error('Network unavailable');}));
});
test('USB setup checks hardware and idle state, sends bounded chunks, and closes on success or refusal',async()=>{
  function port(overrides={},reject=false,dropFirstStatus=false){let stream;return {commands:[],closed:false,open:async function(){this.readable=new ReadableStream({start(c){stream=c;}});this.writable=new WritableStream({write:bytes=>{const cmd=new TextDecoder().decode(bytes).trim();this.commands.push(cmd);if(dropFirstStatus&&this.commands.length===1&&cmd==='STATUS')return;let answer=cmd==='STATUS'?'STATUS '+JSON.stringify({board:27,setup_schema:1,recording:false,upload_busy:false,sd:true,...overrides}):cmd==='SETUP END'?(reject?'SETUP ERROR SD failed':'SETUP OK SAVED'):'SETUP OK DATA';stream.enqueue(new TextEncoder().encode(answer+'\n'));}});},setSignals:async()=>{},close:async function(){this.closed=true;}};}
  const p=port();assert(await sendUploadConfig(p,setup));assert(p.closed);assert.equal(p.commands[1],'SETUP BEGIN');assert.equal(p.commands.at(-1),'SETUP END');
  const data=p.commands.filter(c=>c.startsWith('SETUP DATA '));assert(data.every(c=>c.length<=91));
  const decoded=Buffer.from(data.map(c=>c.slice(11)).join(''),'hex').toString();assert.deepEqual(JSON.parse(decoded),setup);
  for(const state of [{board:1},{setup_schema:0},{recording:true},{sd:false},{upload_busy:true}]){const bad=port(state);await assert.rejects(sendUploadConfig(bad,setup));assert(bad.closed);assert.equal(bad.commands.length,1);}
  const delayed=port({},false,true);assert(await sendUploadConfig(delayed,setup));assert.equal(delayed.commands.filter(c=>c==='STATUS').length,2);assert.equal(delayed.commands.filter(c=>c==='SETUP END').length,1);assert(delayed.closed);
  const bad=port({},true);await assert.rejects(sendUploadConfig(bad,setup),/SD failed/);assert(bad.closed);
});

import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {validateManifest,loadImages,installImages,avatarLine,sendAvatar,sha256} from '../site/flasher.mjs';
const md5=b=>createHash('md5').update(b).digest('hex');
const manifest={schema:1,version:'0.2.0-beta',chip:'ESP32-S3',flashSize:16777216,files:[]};
for(const [name,offset] of [['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]]){const b=new Uint8Array(16).fill(offset/4096);manifest.files.push({name,path:`firmware/${name}`,offset,size:b.length,sha256:await sha256(b),md5:md5(b)});}
assert.equal(validateManifest(manifest),manifest);
for(const mutate of [m=>m.files[0].offset=0x9000,m=>m.files[0].size=0x9000,m=>m.files[1].path='../secret',m=>m.files.pop(),m=>m.chip='ESP32',m=>m.files[0].sha256='x',m=>m.files[0].size=15,m=>m.files.push(m.files[0])]){const m=structuredClone(manifest);mutate(m);assert.throws(()=>validateManifest(m));}
const fetcher=async url=>{const f=manifest.files.find(f=>url.pathname.endsWith(f.path));return {ok:true,arrayBuffer:async()=>new Uint8Array(f.size).fill(f.offset/4096).buffer};};
const images=await loadImages(manifest,fetcher);
await assert.rejects(()=>loadImages(manifest,async()=>({ok:true,arrayBuffer:async()=>new Uint8Array(16).buffer})),/Checksum/);
function fakeLoader(){return {chip:{CHIP_NAME:'ESP32-S3'},detectFlashSize:async()=>'16MB',readFlash:async(a,n)=>{assert.equal(a,0x9000);assert.equal(n,0x5000);return new Uint8Array(n).fill(0x42);},writeFlash:async function(o){this.options=o;assert.equal(o.eraseAll,false);assert.equal(o.fileArray.length,4);for(let i=0;i<4;i++)o.reportProgress(i,16,16);},flashMd5sum:async(a)=>manifest.files.find(f=>f.offset===a).md5,after:async function(mode){assert.equal(mode,'custom_reset');this.restarted=true;}};}
let loader=fakeLoader();const progress=[];await installImages(loader,manifest,images,()=>{},v=>progress.push(v));assert(loader.restarted);assert.equal(progress.at(-1),100);
loader=fakeLoader();loader.chip.CHIP_NAME='ESP32';await assert.rejects(()=>installImages(loader,manifest,images),/Wrong chip/);assert(!loader.options);
loader=fakeLoader();loader.detectFlashSize=async()=>'8MB';await assert.rejects(()=>installImages(loader,manifest,images),/size mismatch/);assert(!loader.options);
loader=fakeLoader();loader.flashMd5sum=async()=>''.padStart(32,'0');await assert.rejects(()=>installImages(loader,manifest,images),/Written checksum/);assert(!loader.restarted);
loader=fakeLoader();let reads=0;loader.readFlash=async(a,n)=>new Uint8Array(n).fill(++reads);await assert.rejects(()=>installImages(loader,manifest,images),/Calibration/);assert(!loader.restarted);
loader=fakeLoader();const bad=structuredClone(images);bad[0].data[0]=99;await assert.rejects(()=>installImages(loader,manifest,bad),/changed/);assert(!loader.options);
const line=avatarLine({name:' Sir Mochi ',base:0,body:'#ffffff',accent:'#ff1234',accessory:3});assert.equal(line,'DCAV2 0 FFFFFF FF1234 0 101029 2 0 0 Sir Mochi');
for(const name of ['', '   ','x\nSTART','more than sixteen chars','🐱'])assert.throws(()=>avatarLine({name,base:0,body:'#ffffff',accent:'#ff1234',accessory:3}));
function mockPort(recording=false,schema=2,bases=5){let controller;return {commands:[],closed:false,open:async function(){this.readable=new ReadableStream({start(c){controller=c;}});this.writable=new WritableStream({write:bytes=>{const command=new TextDecoder().decode(bytes).trim();this.commands.push(command);const response=command==='STATUS'?`STATUS ${JSON.stringify({version:'0.4.0-beta',board:27,recording,avatar_schema:schema,avatar_bases:bases})}\n`:'AVATAR OK\n';controller.enqueue(new TextEncoder().encode(response));}});},setSignals:async()=>{},close:async function(){this.closed=true;}};}
let port=mockPort();assert(await sendAvatar(port,line));assert(port.closed);assert.equal(port.commands[1],'AVATAR SET '+line);
port=mockPort(true);await assert.rejects(()=>sendAvatar(port,line),/STOP & SAVE/);assert(port.closed);assert.equal(port.commands.length,1);
// When packaged firmware exists, independently check every published byte and protected sector boundary.
try{const real=validateManifest(JSON.parse(await readFile(new URL('../site/firmware/manifest.json',import.meta.url))));for(const f of real.files){const b=await readFile(new URL('../site/'+f.path,import.meta.url));assert.equal(b.length,f.size);assert.equal(await sha256(b),f.sha256);assert.equal(md5(b),f.md5);}console.log('Packaged images, SHA-256, MD5 and protected sector ranges passed.');}catch(e){if(e.code!=='ENOENT')throw e;}
console.log('Web installer and avatar protocol tests passed (including failure and refusal paths).');

port=mockPort(false,0);await assert.rejects(()=>sendAvatar(port,line),/0.3/);assert.equal(port.commands.length,1);

for(const base of [3,4]){
  const buddy=avatarLine({name:base===3?'Wisp':'Ember',base,body:'#000000',accent:'#c5bfff',eye:'#edffff'});
  port=mockPort(false,2,5);assert(await sendAvatar(port,buddy));assert.equal(port.commands[1],'AVATAR SET '+buddy);assert(port.closed);
  for(const bases of [undefined,3]){
    port=mockPort(false,2,bases===undefined?null:bases);
    await assert.rejects(()=>sendAvatar(port,buddy),/0.4/);assert.equal(port.commands.length,1);assert(port.closed);
  }
}

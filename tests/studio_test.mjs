import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {DEFAULT_PROFILE,DRAWN_NEW_FIELDS,LOOK_FIELDS,NEW_FIELDS,OPTIONS,PACKS,PERSONA_LINES,PRESETS,V2_LIMITS,WORLDS,applyPack,avatarLine,contrastIssue,parseAvatarLine,profileSchema,randomProfile,seededRng,shufflePack,to565,validateProfile} from '../site/profile.mjs';
import {buddyArt,buddyTones,eyeInk} from '../site/buddy_art.mjs';
import {drawPet,lookOf} from '../site/pet.mjs';
const source=name=>readFile(new URL('../'+name,import.meta.url),'utf8');
const SLOTS=['base','eyes','headwear','eyewear','neckwear','mark','back','held','aura','pack','persona'];
const same=(a,b)=>assert.deepEqual(validateProfile(a),validateProfile(b));

// The website and the firmware must agree on how many choices every slot has.
const limits=await source('include/avatar_limits.h');
const limit=name=>{const m=new RegExp(`\\b${name} = (\\d+)`).exec(limits);assert(m,`avatar_limits.h has no ${name}`);return Number(m[1]);};
const firmware={base:'Bases',eyes:'Eyes',headwear:'Headwear',eyewear:'Eyewear',neckwear:'Neckwear',mark:'Marks',back:'Backs',held:'Held',aura:'Auras',pack:'Packs',persona:'Personas'};
assert.deepEqual(Object.keys(OPTIONS),SLOTS);
for(const slot of SLOTS)assert.equal(OPTIONS[slot].length,limit(firmware[slot]),`${slot} count differs from avatar_limits.h`);
for(const [slot,count] of Object.entries(V2_LIMITS))assert.equal(count,limit('V2'+firmware[slot]));
for(const slot of SLOTS)assert.equal(new Set(OPTIONS[slot]).size,OPTIONS[slot].length,`${slot} has a repeated name`);
assert.equal(['base','eyes','headwear','eyewear','neckwear','mark','back','held','aura'].reduce((n,slot)=>n*OPTIONS[slot].length,1),90316800);

// Style packs: the website's signature looks are the firmware's (include/profile.h).
const packRows=[...(await source('include/profile.h')).matchAll(/\{"([^"]+)",\s*0x([0-9a-f]{6}), 0x([0-9a-f]{6}), 0x([0-9a-f]{6}),((?:\s*\d+,){8}\s*\d+)\}/g)];
assert.equal(packRows.length,OPTIONS.pack.length);
packRows.forEach((row,id)=>{
  assert.equal(row[1],OPTIONS.pack[id]);
  if(!id){assert.equal(PACKS[0],null);return;}
  const [eyes,headwear,eyewear,neckwear,mark,back,held,aura,backdrop]=row[5].split(',').map(Number);
  assert.deepEqual(PACKS[id].look,{body:'#'+row[2],accent:'#'+row[3],eye:'#'+row[4],eyes,headwear,eyewear,neckwear,mark,back,held,aura},`${row[1]} look differs from PackStyles`);
  assert.equal(PACKS[id].world,backdrop);assert.equal(PACKS[id].name,row[1]);assert(WORLDS[PACKS[id].world]);
  assert.deepEqual(Object.keys(PACKS[id].look).sort(),[...LOOK_FIELDS].sort());
  // A pack dresses the current shape: it never renames, reshapes or re-voices the buddy.
  for(let base=0;base<OPTIONS.base.length;base++){
    const mine={...DEFAULT_PROFILE,name:'Keep me',base,persona:2,eyewear:5,held:3},dressed=applyPack(mine,id);
    assert.equal(dressed.name,'Keep me');assert.equal(dressed.base,base);assert.equal(dressed.persona,2);assert.equal(dressed.pack,id);
    for(const field of LOOK_FIELDS)assert.equal(dressed[field],PACKS[id].look[field]);
    same(parseAvatarLine(avatarLine(dressed)),dressed);
    assert(avatarLine(dressed).startsWith('DCAV3 '));
  }
  assert.deepEqual(PACKS[id].palettes[0],{body:PACKS[id].look.body,accent:PACKS[id].look.accent,eye:PACKS[id].look.eye});
  for(const [slot,pool] of Object.entries(PACKS[id].pool)){assert(pool.includes(PACKS[id].look[slot]),`${row[1]} pool lacks its own ${slot}`);for(const v of pool)assert(Number.isInteger(v)&&v>=0&&v<OPTIONS[slot].length);}
});
for(const bad of [0,6,-1,1.5,'1',undefined])assert.throws(()=>applyPack(DEFAULT_PROFILE,bad));
// Persona samples are lines those buddies really say.
const lines=await source('include/personality.h');
OPTIONS.persona.slice(1).forEach((who,i)=>assert(lines.includes(`Who::${who}, false, "${PERSONA_LINES[i+1]}"`),`${who} never says: ${PERSONA_LINES[i+1]}`));

// Starters round-trip and keep distinct labels.
for(const p of PRESETS)same(parseAvatarLine(avatarLine(p)),p);
assert.equal(new Set(PRESETS.map(p=>p.name)).size,PRESETS.length);

// Every slot, at its edges: the last option round-trips, one past it is refused, and so is
// anything that is not a whole number.
for(const slot of SLOTS){
  // The pack tag only travels on a DCAV3 line, so give that case one piece of new gear.
  const last=OPTIONS[slot].length-1,p={...DEFAULT_PROFILE,[slot]:last,...(slot==='pack'?{mark:1}:{})};
  assert.equal(parseAvatarLine(avatarLine(p))[slot],last);
  for(const bad of [last+1,-1,1.5,NaN,Infinity,String(last),null])assert.throws(()=>avatarLine({...DEFAULT_PROFILE,[slot]:bad}),/Unknown/,`${slot}=${bad}`);
  const numbers=SLOTS.map(s=>s===slot?last+1:0);
  assert.throws(()=>parseAvatarLine(`DCAV3 ${numbers[0]} FFFFFF 000000 123456 ${numbers.slice(1).join(' ')} Edge`),`${slot} one past the end`);
  numbers[SLOTS.indexOf(slot)]=last;
  assert.equal(parseAvatarLine(`DCAV3 ${numbers[0]} FFFFFF 000000 123456 ${numbers.slice(1).join(' ')} Edge`)[slot],last);
}
for(const update of [{eye:'#fff'},{body:'fff3d2'},{accent:'#gggggg'},{name:'<script>'},{name:' '},{name:''},{name:'seventeen letters'},{name:5}])assert.throws(()=>avatarLine({...DEFAULT_PROFILE,...update}));

// Which line a design is written as: DCAV2 while it fits there, so plain designs still load on
// firmware 0.3 - 0.5; DCAV3 as soon as one new slot or one new option is used.
assert.equal(avatarLine(DEFAULT_PROFILE),'DCAV2 0 FFF3D2 FF88B6 0 101029 0 0 0 My buddy');
const classic={...DEFAULT_PROFILE,base:4,eyes:5,headwear:5,eyewear:3,neckwear:3,name:'Old max'};
assert.equal(profileSchema(classic),2);assert.equal(avatarLine(classic),'DCAV2 4 FFF3D2 FF88B6 5 101029 5 3 3 Old max');
for(const [slot,count] of Object.entries(V2_LIMITS)){const p={...classic,[slot]:count};assert.equal(profileSchema(p),3);assert(avatarLine(p).startsWith('DCAV3 '));}
for(const slot of DRAWN_NEW_FIELDS){const p={...classic,[slot]:1};assert.equal(profileSchema(p),3);assert(avatarLine(p).startsWith('DCAV3 '));}
// The style-pack tag draws nothing, so alone it never forces DCAV3: the design is written as the
// classic line and only the tag is left out. Trying a pack and then taking its gear off again
// must leave a design old firmware can read.
assert.deepEqual(NEW_FIELDS.filter(f=>!DRAWN_NEW_FIELDS.includes(f)),['pack']);
{const tagged={...classic,pack:1};assert.equal(profileSchema(tagged),2);assert.equal(avatarLine(tagged),avatarLine(classic));assert.equal(parseAvatarLine(avatarLine(tagged)).pack,0);
 const undressed={...applyPack(classic,1),eyes:0,headwear:0,eyewear:0,neckwear:0,mark:0,back:0,held:0,aura:0};assert.equal(undressed.pack,1);assert(avatarLine(undressed).startsWith('DCAV2 '));
 assert.equal(parseAvatarLine(avatarLine({...tagged,mark:1})).pack,1);}
assert.equal(avatarLine({...DEFAULT_PROFILE,name:'Nyx the 2nd',base:3,body:'#17141f',accent:'#b3123c',eye:'#f2e9ff',eyes:9,headwear:7,neckwear:5,mark:4,back:2,held:4,aura:5,pack:3,persona:1}),
  'DCAV3 3 17141F B3123C F2E9FF 9 7 0 5 4 2 4 5 3 1 Nyx the 2nd');
same(parseAvatarLine('dcav3 3 17141f b3123c f2e9ff 9 7 0 5 4 2 4 5 3 1 Nyx the 2nd\r\n'),{name:'Nyx the 2nd',base:3,body:'#17141f',accent:'#b3123c',eye:'#f2e9ff',eyes:9,headwear:7,eyewear:0,neckwear:5,mark:4,back:2,held:4,aura:5,pack:3,persona:1});

// A seeded sample instead of every combination (there are 90 million): round trips, line
// length, and what firmware 0.3 - 0.5 would make of each line.
const legacy=/^DCAV2 ([0-4]) ([A-F0-9]{6}) ([A-F0-9]{6}) ([0-5]) ([A-F0-9]{6}) ([0-5]) ([0-3]) ([0-3]) ([A-Za-z0-9 _-]{1,16})$/;
const rng=seededRng(20260),names=['a','My buddy','Sixteen chars ok','x_y-z 9','Scout 3 2','0'];
const roll=n=>Math.floor(rng()*n),colour=()=>'#'+roll(0x1000000).toString(16).padStart(6,'0');
let classicLines=0,newLines=0,longest=0;
for(let i=0;i<4000;i++){
  const modest=i%3===0,p={...DEFAULT_PROFILE,name:names[roll(names.length)],body:colour(),accent:colour(),eye:colour()};
  for(const slot of SLOTS)p[slot]=NEW_FIELDS.includes(slot)?(modest?0:roll(OPTIONS[slot].length)):roll(modest&&V2_LIMITS[slot]||OPTIONS[slot].length);
  const line=avatarLine(p);longest=Math.max(longest,line.length);
  same(parseAvatarLine(line),profileSchema(p)===2?{...p,pack:0}:p);assert.equal(avatarLine(parseAvatarLine(line)),line);
  if(profileSchema(p)===2){classicLines++;const m=legacy.exec(line);assert(m,line);assert.equal(m[9],p.name);assert.equal(Number(m[6]),p.headwear);}
  else{newLines++;assert(line.startsWith('DCAV3 '));assert(!legacy.test(line));}
}
assert(classicLines>1000&&newLines>2000);assert(longest<=66);
assert.equal(avatarLine({...DEFAULT_PROFILE,name:'Sixteen chars ok',base:4,body:'#ffffff',accent:'#ffffff',eye:'#ffffff',eyes:9,headwear:11,eyewear:7,neckwear:7,mark:7,back:6,held:5,aura:6,pack:5,persona:5}).length,66);

// Imports: older files migrate; anything malformed is refused, as the firmware refuses it.
const old=parseAvatarLine('DCAV1 2 112233 FFFFFF 2 Old Orbit\n');
assert.equal(old.neckwear,1);assert.equal(old.headwear,0);assert.equal(old.eye,'#84f7b5');assert.equal(old.mark,0);assert.equal(old.persona,0);
assert.equal(parseAvatarLine('DCAV1 0 112233 FFFFFF 3 Old Crown').headwear,2);
assert.equal(parseAvatarLine('DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Scout 3 2').name,'Scout 3 2');
for(const input of ['<svg onload=alert(1)>','DCAV2 0 FF ZZ 1 0 0 0 0 Hi','DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Good\nSTART','x'.repeat(129),
  'DCAV1 3 000000 FFFFFF 0 Nope','DCAV1 0 000000 FFFFFF 4 Nope','DCAV2 5 FFFFFF FFFFFF 0 000000 0 0 0 Nope',
  'DCAV2 0 FFFFFF FFFFFF 6 000000 0 0 0 Nope','DCAV2 0 FFFFFF FFFFFF 0 000000 6 0 0 Nope','DCAV2 0 FFFFFF FFFFFF 0 000000 0 4 0 Nope','DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 4 Nope',
  'DCAV2 00 FFFFFF FFFFFF 0 000000 0 0 0 Padded','DCAV2 0  FFFFFF FFFFFF 0 000000 0 0 0 Spaces','DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Bad;name',
  'DCAV3 0 FFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Short','DCAV3 0 FFFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Long','DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 Missing',
  'DCAV3 0 FFFFFF FFFFFF 000000 00 0 0 0 0 0 0 0 0 0 Padded','DCAV3 0 FFFFFF FFFFFF 000000 -1 0 0 0 0 0 0 0 0 0 Signed','DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 Extra',
  'DCAV3 0 FFFFFF FFFFFF 0 112233 0 0 0 Nope','DCAV4 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Future','DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Good\nAVATAR SET x',5,null])assert.throws(()=>parseAvatarLine(input),undefined,String(input));

// Every example a user can copy (the docs and the SD card sample) is a valid, canonical line.
let examples=0;
for(const file of ['docs/AVATAR-STUDIO.md','docs/QUICKSTART.md','sdcard/dashchan/avatar.example.cfg'])for(const line of (await source(file)).split('\n').filter(l=>/^DCAV\d /.test(l))){
  const p=parseAvatarLine(line);examples++;
  if(!line.startsWith('DCAV1 '))assert.equal(avatarLine(p),line,`${file}: not how the studio would write it`);
}
assert(examples>=6);
assert.equal(profileSchema(parseAvatarLine(await source('sdcard/dashchan/avatar.example.cfg'))),3);

// Eye contrast: the studio warns exactly when the robot would have to rescue the eyes, or
// when they sit too close to what is behind them; the suggested colour always fixes it.
assert.equal(contrastIssue(DEFAULT_PROFILE),null);
for(const [update,level] of [[{body:'#000000',eye:'#000000'},'swap'],[{base:2,eye:'#101029'},'swap'],[{body:'#ffffff',eye:'#ffffff'},'swap'],[{base:2,eye:'#6b3fa0'},'swap'],[{body:'#808080',eye:'#808080'},'low'],[{body:'#ff0000',eye:'#e01010'},'low']]){
  const p={...DEFAULT_PROFILE,...update},issue=contrastIssue(p);
  assert.equal(issue?.level,level,JSON.stringify(update));assert(issue.message.length>20);
  assert.equal(contrastIssue({...p,eye:issue.fix}),null);
}
assert.equal(contrastIssue({...DEFAULT_PROFILE,base:2,body:'#000000',eye:'#9affdd'}),null); // Orbit's screen is dark whatever the shell.

// Surprise me: seeded, so it can be tested. Everything changes except the name.
const sample=[];
for(let seed=1;seed<=600;seed++){
  const mine={...DEFAULT_PROFILE,name:'Captain Mochi',pack:3},p=randomProfile(seededRng(seed),mine);
  assert.deepEqual(p,randomProfile(seededRng(seed),mine),'same seed, same buddy');
  assert.equal(p.name,'Captain Mochi');assert.deepEqual(p,validateProfile(p));
  assert.equal(contrastIssue(p),null,`seed ${seed}: eyes must read`);
  assert.equal(eyeInk(p.base,true,to565(p.eye),to565(p.body)),to565(p.eye));
  assert.notEqual(to565(p.accent),to565(p.body));assert.equal(p.pack,0);
  same(parseAvatarLine(avatarLine(p)),p);sample.push(p);
}
for(const slot of SLOTS.filter(s=>s!=='pack'))assert.equal(new Set(sample.map(p=>p[slot])).size,OPTIONS[slot].length,`Surprise me never reaches every ${slot}`);
for(const field of ['body','accent','eye'])assert(new Set(sample.map(p=>p[field])).size>300,`Surprise me barely varies ${field}`);
assert.notDeepEqual(randomProfile(seededRng(1)),randomProfile(seededRng(2)));
for(const name of ['Bad;name','','   ',undefined])assert.equal(randomProfile(seededRng(7),{...DEFAULT_PROFILE,name}).name,'My buddy');
assert.equal(randomProfile(seededRng(7),{...DEFAULT_PROFILE,name:'  Padded  '}).name,'Padded');
// Shuffle within pack: that pack's palettes and wardrobe, on the same shape with the same name.
for(let seed=1;seed<=400;seed++)for(const id of [0,1,2,3,4,5]){
  const mine={...DEFAULT_PROFILE,name:'Scout',base:seed%5,persona:seed%6,pack:id},p=shufflePack(seededRng(seed*7+id),mine);
  assert.equal(p.name,'Scout');assert.equal(p.base,mine.base);assert.equal(p.persona,mine.persona);
  assert(p.pack>=1&&p.pack<PACKS.length);if(id)assert.equal(p.pack,id);
  const style=PACKS[p.pack];
  for(const [slot,pool] of Object.entries(style.pool))assert(pool.includes(p[slot]));
  assert(style.palettes.some(t=>t.body===p.body));assert.equal(contrastIssue(p),null);
  assert.deepEqual(p,shufflePack(seededRng(seed*7+id),mine));
}

// The renderer: every option of every slot draws without error on a stand-in canvas, in every mood.
let calls=0;
const g=new Proxy({},{get:(_,name)=>name==='createLinearGradient'?()=>({addColorStop(){}}):()=>{calls++;},set:()=>true});
const canvas={width:440,height:330,getContext:()=>g};
for(const slot of SLOTS.filter(s=>s!=='pack'&&s!=='persona'))for(let option=0;option<OPTIONS[slot].length;option++)for(const mood of [0,1,2,3,4,5,6]){
  const before=calls;drawPet(canvas,{...DEFAULT_PROFILE,base:option%5,[slot]:option,custom:true,mood,field:mood%3,world:mood%5===4?null:mood%4,zoom:1.5,focusY:-4},mood*731+option*97);
  assert(calls-before>100);
}
assert.deepEqual(lookOf({...DEFAULT_PROFILE,custom:true,aura:3}),{base:0,custom:true,body:to565('#fff3d2'),accent:to565('#ff88b6'),eye:to565('#101029'),eyes:0,headwear:0,eyewear:0,neckwear:0,mark:0,back:0,held:0,aura:3});

// The generated web art makes exactly the calls the firmware's art makes. tools/test.py writes
// the firmware's side (480 looks) to build/art-trace.txt.
let parity='not checked (run python tools/test.py first)';
try{
  const trace=(await source('build/art-trace.txt')).trimEnd().split('\n');
  let out=[],looks=0;
  const scribe=tag=>({oval:(...a)=>out.push(`${tag} oval ${a.join(' ')}`),box:(...a)=>out.push(`${tag} box ${a.join(' ')}`),tri:(...a)=>out.push(`${tag} tri ${a.join(' ')}`),
    line2:(x0,y0,x1,y1,c,w=2)=>out.push(`${tag} line2 ${x0} ${y0} ${x1} ${y1} ${c} ${w}`),bar:(...a)=>out.push(`${tag} bar ${a.join(' ')}`)});
  for(let i=0;i<trace.length;){
    const fields=trace[i].split(' ');assert.equal(fields[0],'look');
    const [base,custom,body,accent,eye,eyes,headwear,eyewear,neckwear,mark,back,held,aura,mood,gaze,blink,kit,phase]=fields.slice(1).map(Number);
    out=[];buddyArt(scribe('p'),scribe('rim'),scribe('edge'),{base,custom:!!custom,body,accent,eye,eyes,headwear,eyewear,neckwear,mark,back,held,aura},buddyTones(base,!!custom,body,accent),mood,gaze,!!blink,kit,phase);
    let j=i+1;while(j<trace.length&&!trace[j].startsWith('look '))j++;
    assert.deepEqual(out,trace.slice(i+1,j),`Web art differs from firmware art for: ${trace[i]}. If avatars.h changed, rerun python tools/test.py.`);
    looks++;i=j;
  }
  assert(looks>=400);parity=`${looks} looks identical`;
}catch(e){if(e.code!=='ENOENT')throw e;}
console.log(`Studio profiles: per-slot bounds, DCAV1/2/3, a 4,000-profile seeded sample, packs, Surprise me, Shuffle, contrast and rendering passed. Firmware art parity: ${parity}.`);

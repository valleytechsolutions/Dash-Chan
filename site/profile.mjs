// Versioned appearance only. Importing a profile never changes audio or radio settings.
// Slot names and counts mirror include/avatar_limits.h; tests/studio_test.mjs compares them.
import {eyeInk} from './buddy_art.mjs';
export const DEFAULT_PROFILE = Object.freeze({name:'My buddy',base:0,body:'#fff3d2',accent:'#ff88b6',eyes:0,eye:'#101029',headwear:0,eyewear:0,neckwear:0,mark:0,back:0,held:0,aura:0,pack:0,persona:0});
export const OPTIONS = {
  base:['Mochi · cat','Sprout · dragon','Orbit · robot','Wisp · ghost','Ember · flame'],
  eyes:['Classic','Sparkle','Sleepy','Cat eyes','Pixel','Wink','Heart','Star','Glitch X','Half-lid'],
  headwear:['None','Bow','Crown','Beanie','Flower','Scout cap','Cat-ear headphones','Witch hat','Halo','Horns','Antenna headband','Hood'],
  eyewear:['None','Round glasses','Shades','Cyber visor','Heart shades','Monocle','Eyepatch','Scan-line HUD'],
  neckwear:['None','Scarf','Bandana','Bow tie','Choker','Spiked collar','Bell collar','Neck headphones'],
  mark:['None','Blush hearts','Star cheek','Whisker stripes','Tear','Circuit trace','Stitch scar','Barcode'],
  back:['None','Angel wings','Bat wings','Cape','Backpack antenna','Jet thrusters','Fairy wings'],
  held:['None','Boba tea','Lollipop','Game pad','Lantern','Plush bear'],
  aura:['None','Sparkles','Hearts','Glitch pixels','Embers','Bats','Petals'],
  pack:['None','Kawaii pastel','Cyberpunk neon','Goth','Vaporwave','Retro terminal'],
  persona:['Match shape','Mochi','Sprout','Orbit','Wisp','Ember'],
};
// What a DCAV2 line can carry. Firmware 0.3 - 0.5 reads nothing newer.
export const V2_LIMITS = Object.freeze({eyes:6,headwear:6,eyewear:4,neckwear:4});
export const NEW_FIELDS = Object.freeze(['mark','back','held','aura','pack','persona']);
// The DCAV3 fields that change what the robot draws or says. The style-pack number is only a
// tag (it draws nothing), so it never forces the newer format by itself: a design that otherwise
// fits DCAV2 is written as DCAV2 and the tag is left out.
export const DRAWN_NEW_FIELDS = Object.freeze(NEW_FIELDS.filter(f=>f!=='pack'));
// Everything a style pack or a shuffle may restyle. Name, shape and persona are the owner's.
export const LOOK_FIELDS = Object.freeze(['body','accent','eye','eyes','headwear','eyewear','neckwear','mark','back','held','aura']);
export const WORLDS = ['Signal City','Sunset Run','Star Patrol','Pixel Garden'];
// One line each buddy really says (include/personality.h), so a persona can be heard before it is chosen.
export const PERSONA_LINES = ['',"I'd knock a router off a shelf. For science.",'Counting my signal hoard. Again.','Running diagnostics. Result: cute.','I walk through walls. So does 2.4 GHz.',"I'm lit. Literally."];

// Style packs. `look` is the signature outfit and must match PackStyles in include/profile.h
// (the robot's own Style pack button applies the same one). `palettes` and `pool` are what
// Shuffle draws from; the first palette is the signature colours.
const pack=(name,blurb,world,palettes,gear,pool)=>({name,blurb,world,look:{...palettes[0],...gear},palettes,pool});
const tones=(body,accent,eye)=>({body,accent,eye});
export const PACKS = [
  null,
  pack('Kawaii pastel','Soft, sweet, a bit sparkly',3,
    [tones('#ffd9ec','#ff6fae','#6b3fa0'),tones('#d9f2ff','#ff9ecb','#3a4f8f'),tones('#fff3c4','#ff8fb3','#7a4a2a'),tones('#e6dcff','#ffa0d0','#4a3a8a')],
    {eyes:1,headwear:1,eyewear:0,neckwear:6,mark:1,back:6,held:0,aura:2},
    {eyes:[0,1,5,6,7],headwear:[0,1,4,8,2],eyewear:[0,0,1,4],neckwear:[0,6,3,1],mark:[1,1,2,0],back:[0,6,1],held:[0,1,2,5],aura:[1,2,6]}),
  pack('Cyberpunk neon','Night city, loud colours',0,
    [tones('#1a1433','#ff2bd6','#2bf5ff'),tones('#101a2e','#00f0ff','#ff4fd8'),tones('#22103a','#f9f002','#00ffd0'),tones('#0f1f2b','#ff5a36','#7dfcff')],
    {eyes:4,headwear:6,eyewear:7,neckwear:0,mark:5,back:5,held:0,aura:3},
    {eyes:[4,8,3,0],headwear:[0,6,10,3],eyewear:[7,3,2,6],neckwear:[0,4,7,5],mark:[5,7,0],back:[5,4,0],held:[0,3,1],aura:[3,3,4,1]}),
  pack('Goth','Midnight, lace and bats',2,
    [tones('#17141f','#b3123c','#f2e9ff'),tones('#1b1026','#8f5bff','#ffe9f4'),tones('#22161a','#d94f70','#f7f0dc'),tones('#3a2f4a','#c9a4ff','#fff6fb')],
    {eyes:9,headwear:7,eyewear:0,neckwear:5,mark:4,back:2,held:4,aura:5},
    {eyes:[9,3,2,8],headwear:[7,9,11,0,1],eyewear:[0,0,6,5],neckwear:[5,4,3],mark:[4,6,0],back:[2,3,0],held:[4,0,5],aura:[5,5,4,0]}),
  pack('Vaporwave','Sunset grids and dreamy hues',1,
    [tones('#b967ff','#01cdfe','#fffb96'),tones('#ff71ce','#05ffa1','#2a1a5e'),tones('#7ad7f0','#ff6ad5','#3b1f7a'),tones('#ffb3e6','#8c6bff','#143a5e')],
    {eyes:7,headwear:4,eyewear:0,neckwear:0,mark:2,back:1,held:1,aura:1},
    {eyes:[7,1,5,0],headwear:[4,8,0,3],eyewear:[0,2,4,1],neckwear:[0,1,2],mark:[2,1,0],back:[1,6,0,3],held:[1,0,2],aura:[1,6,2]}),
  pack('Retro terminal','Green phosphor, blinking cursor',0,
    [tones('#0d1f12','#33ff66','#8dffa8'),tones('#1f1606','#ffb000','#ffd98a'),tones('#101820','#5ad1ff','#c8f0ff'),tones('#d8d2c0','#2f6f3a','#1c2a1e')],
    {eyes:4,headwear:10,eyewear:0,neckwear:0,mark:7,back:4,held:3,aura:3},
    {eyes:[4,0,2,8],headwear:[10,0,5],eyewear:[0,3,5,1],neckwear:[0,3],mark:[7,5,0],back:[4,0],held:[3,0],aura:[3,0,4]}),
];
// Starter buddies. Their names are only labels: loading one keeps the name you chose.
export const PRESETS = [
  {...DEFAULT_PROFILE,name:'Wisp',base:3,body:'#000000',accent:'#c5bfff',eye:'#edffff',eyes:0},
  {...DEFAULT_PROFILE,name:'Ember',base:4,body:'#ff4d24',accent:'#ffe040',eye:'#201029',eyes:0},
  {...DEFAULT_PROFILE,name:'Signal scout',headwear:5,neckwear:2,eyes:1,accent:'#ffad73'},
  {...DEFAULT_PROFILE,name:'Neon listener',base:2,body:'#a8a0ff',accent:'#7af3ce',eye:'#9affdd',eyes:4,eyewear:3,neckwear:1},
  {...DEFAULT_PROFILE,name:'Petal patrol',base:1,body:'#a5edc0',accent:'#ff9dc8',eyes:3,eye:'#245549',headwear:4},
  {...DEFAULT_PROFILE,name:'Mochi royale',body:'#ffe3a3',accent:'#dba4ff',eyes:5,headwear:2,neckwear:3},
];
const NAME_ERROR='Use a name with 1–16 letters, numbers, spaces, - or _.';
export function validateProfile(input){
  if(!input||typeof input!=='object')throw Error('Choose a valid buddy profile.');
  const p={...DEFAULT_PROFILE,...input};
  if(typeof p.name!=='string')throw Error(NAME_ERROR);
  p.name=p.name.trim();
  if(!/^[A-Za-z0-9 _-]{1,16}$/.test(p.name))throw Error(NAME_ERROR);
  for(const field of ['body','accent','eye']){if(typeof p[field]!=='string'||!/^#[a-f0-9]{6}$/i.test(p[field]))throw Error('Colors must use six hexadecimal digits.');p[field]=p[field].toLowerCase();}
  for(const [field,choices] of Object.entries(OPTIONS))if(!Number.isInteger(p[field])||p[field]<0||p[field]>=choices.length)throw Error(`Unknown ${field} style.`);
  return Object.fromEntries(Object.keys(DEFAULT_PROFILE).map(k=>[k,p[k]]));
}
// 2 while the design fits the older DCAV2 line (so it still loads on firmware 0.3 - 0.5), else 3.
// A style-pack tag alone does not count; see DRAWN_NEW_FIELDS.
export function profileSchema(input){
  const p=validateProfile(input);
  return DRAWN_NEW_FIELDS.every(f=>p[f]===0)&&Object.entries(V2_LIMITS).every(([f,count])=>p[f]<count)?2:3;
}
export function avatarLine(input){
  // Keep the original API useful to older integrations that supplied one accessory.
  const source={...input};
  if(Object.hasOwn(source,'accessory')){
    if(!Number.isInteger(source.accessory)||source.accessory<0||source.accessory>3)throw Error('Unknown legacy accessory.');
    source.headwear??=source.accessory===1?1:source.accessory===3?2:0;
    source.neckwear??=source.accessory===2?1:0;
  }
  const p=validateProfile(source),hex=v=>v.slice(1).toUpperCase();
  if(profileSchema(p)===2)return `DCAV2 ${p.base} ${hex(p.body)} ${hex(p.accent)} ${p.eyes} ${hex(p.eye)} ${p.headwear} ${p.eyewear} ${p.neckwear} ${p.name}`;
  return `DCAV3 ${p.base} ${hex(p.body)} ${hex(p.accent)} ${hex(p.eye)} ${p.eyes} ${p.headwear} ${p.eyewear} ${p.neckwear} ${p.mark} ${p.back} ${p.held} ${p.aura} ${p.pack} ${p.persona} ${p.name}`;
}
const N='(0|[1-9][0-9]?)',H='([a-f0-9]{6})',NAME='([A-Za-z0-9 _-]{1,16})';
const V3=new RegExp(`^DCAV3 ${N} ${H} ${H} ${H}${` ${N}`.repeat(10)} ${NAME}$`,'i');
const V2=new RegExp(`^DCAV2 ${N} ${H} ${H} ${N} ${H} ${N} ${N} ${N} ${NAME}$`,'i');
const V1=new RegExp(`^DCAV1 ${N} ${H} ${H} ${N} ${NAME}$`,'i');
const IMPORT_ERROR='Choose a valid Dash-Chan avatar.cfg file (DCAV1, DCAV2 or DCAV3).';
export function parseAvatarLine(text){
  if(typeof text!=='string'||text.length>128)throw Error('That avatar file is too large or invalid.');
  const line=text.trim();
  let m=V3.exec(line);
  if(m){
    const [base,eyes,headwear,eyewear,neckwear,mark,back,held,aura,pack,persona]=[m[1],...m.slice(5,15)].map(Number);
    return validateProfile({base,body:'#'+m[2],accent:'#'+m[3],eye:'#'+m[4],eyes,headwear,eyewear,neckwear,mark,back,held,aura,pack,persona,name:m[15]});
  }
  m=V2.exec(line);
  if(m){
    const p={base:+m[1],body:'#'+m[2],accent:'#'+m[3],eyes:+m[4],eye:'#'+m[5],headwear:+m[6],eyewear:+m[7],neckwear:+m[8],name:m[9]};
    // A DCAV2 line may only hold what old firmware accepts; newer options need DCAV3.
    for(const [field,count] of Object.entries(V2_LIMITS))if(p[field]>=count)throw Error(IMPORT_ERROR);
    return validateProfile(p);
  }
  m=V1.exec(line);
  if(m){
    const base=+m[1],accessory=+m[4];
    if(base>2||accessory>3)throw Error(IMPORT_ERROR);
    return validateProfile({base,body:'#'+m[2],accent:'#'+m[3],eye:base===2?'#84f7b5':'#101029',headwear:accessory===1?1:accessory===3?2:0,neckwear:accessory===2?1:0,name:m[5]});
  }
  throw Error(IMPORT_ERROR);
}

// The outfit a pack puts on. Keeps the shape, the name and the persona.
export function applyPack(profile,id){
  const p=validateProfile(profile);
  if(!Number.isInteger(id)||id<1||id>=PACKS.length)throw Error('Unknown style pack.');
  return validateProfile({...p,...PACKS[id].look,pack:id});
}

// Colour maths on the robot's own RGB565, so the studio judges what the LCD will show.
export const to565=hex=>{const c=parseInt(hex.slice(1),16);return ((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);};
const luma=c=>((c>>11)&31)*2+((c>>5)&63)*2+(c&31);
const hex=(r,g,b)=>'#'+[r,g,b].map(v=>Math.max(0,Math.min(255,Math.round(v))).toString(16).padStart(2,'0')).join('');
// What the eyes sit on: Orbit's dark screen, or the body.
const surface=p=>p.base===2?0x10c7:to565(p.body);
// Null when the eyes read clearly. Otherwise how they fail and a colour that fixes it.
export function contrastIssue(profile){
  const p=validateProfile(profile),eye=to565(p.eye),behind=surface(p),where=p.base===2?"Orbit's dark screen":'the body';
  const fix=luma(behind)>110?'#101029':'#edffff';
  if(eyeInk(p.base,true,eye,to565(p.body))!==eye)return {level:'swap',fix,message:`This eye colour would vanish into ${where}, so the robot draws the eyes in a rescue colour instead.`};
  if(Math.abs(luma(eye)-luma(behind))<45)return {level:'low',fix,message:`The eyes are close to the colour of ${where} and may be hard to see.`};
  return null;
}
// A small deterministic generator, so randomising can be tested: seededRng(1)() is always the same.
export function seededRng(seed){
  let a=seed>>>0;
  return ()=>{a=(a+0x6d2b79f5)>>>0;let t=a;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);return ((t^(t>>>14))>>>0)/4294967296;};
}
function hsl(h,s,l){
  const f=n=>{const k=(n+h/30)%12;return l-s*Math.min(l,1-l)*Math.max(-1,Math.min(k-3,9-k,1));};
  return hex(f(0)*255,f(8)*255,f(4)*255);
}
const pick=(rng,list)=>list[Math.floor(rng()*list.length)];
function readable(p){
  // Never hand back eyes the robot would have to rescue, or an accent lost against the body.
  if(contrastIssue(p))p.eye=contrastIssue(p).fix;
  if(Math.abs(luma(to565(p.accent))-luma(to565(p.body)))<30)p.accent=luma(to565(p.body))>110?'#c2185b':'#ffd166';
  return p;
}
// Surprise me: a whole new buddy. Everything changes except the name.
export function randomProfile(rng=Math.random,current=DEFAULT_PROFILE){
  const p={...DEFAULT_PROFILE,name:validName(current?.name)};
  const count=key=>OPTIONS[key].length,any=key=>Math.floor(rng()*count(key));
  // Gear slots are empty about half the time, so a surprise is an outfit and not a pile.
  const maybe=key=>rng()<.5?0:1+Math.floor(rng()*(count(key)-1));
  p.base=any('base');p.eyes=any('eyes');p.persona=any('persona');
  for(const key of ['headwear','eyewear','neckwear','mark','back','held','aura'])p[key]=maybe(key);
  const hue=rng()*360,style=Math.floor(rng()*3);
  p.body=style===0?hsl(hue,.75,.86):style===1?hsl(hue,.35,.12):hsl(hue,.8,.62);
  p.accent=hsl((hue+[150,180,210,40][Math.floor(rng()*4)])%360,.85,style===1?.6:.55);
  p.eye=style===1?hsl((hue+60)%360,.9,.85):hsl((hue+200)%360,.5,.16);
  return validateProfile(readable(p));
}
// Shuffle within a pack: its palettes and its wardrobe, on the current shape. Without a pack
// tag one is drawn first.
export function shufflePack(rng=Math.random,current=DEFAULT_PROFILE){
  const p=validateProfile({...current,name:validName(current?.name)});
  const id=p.pack||1+Math.floor(rng()*(PACKS.length-1)),style=PACKS[id];
  Object.assign(p,pick(rng,style.palettes),{pack:id});
  for(const [key,choices] of Object.entries(style.pool))p[key]=pick(rng,choices);
  return validateProfile(readable(p));
}
function validName(name){
  const text=typeof name==='string'?name.trim():'';
  return /^[A-Za-z0-9 _-]{1,16}$/.test(text)?text:DEFAULT_PROFILE.name;
}

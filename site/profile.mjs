// Versioned appearance only. Importing a profile never changes audio or radio settings.
export const DEFAULT_PROFILE = Object.freeze({name:'My buddy',base:0,body:'#fff3d2',accent:'#ff88b6',eyes:0,eye:'#101029',headwear:0,eyewear:0,neckwear:0});
export const OPTIONS = {
  base:['Mochi · cat','Sprout · dragon','Orbit · robot','Wisp · ghost','Ember · flame'],
  eyes:['Classic','Sparkle','Sleepy','Cat eyes','Pixel','Wink'],
  headwear:['None','Bow','Crown','Beanie','Flower','Scout cap'],
  eyewear:['None','Round glasses','Shades','Cyber visor'],
  neckwear:['None','Scarf','Bandana','Bow tie'],
};
export const PRESETS = [
  {...DEFAULT_PROFILE,name:'Wisp',base:3,body:'#000000',accent:'#c5bfff',eye:'#edffff',eyes:0},
  {...DEFAULT_PROFILE,name:'Ember',base:4,body:'#ff4d24',accent:'#ffe040',eye:'#201029',eyes:0},
  {...DEFAULT_PROFILE,name:'Signal scout',headwear:5,neckwear:2,eyes:1,accent:'#ffad73'},
  {...DEFAULT_PROFILE,name:'Neon listener',base:2,body:'#a8a0ff',accent:'#7af3ce',eye:'#9affdd',eyes:4,eyewear:3,neckwear:1},
  {...DEFAULT_PROFILE,name:'Petal patrol',base:1,body:'#a5edc0',accent:'#ff9dc8',eyes:3,eye:'#245549',headwear:4},
  {...DEFAULT_PROFILE,name:'Mochi royale',body:'#ffe3a3',accent:'#dba4ff',eyes:5,headwear:2,neckwear:3},
];
export function validateProfile(input){
  if(!input||typeof input!=='object')throw Error('Choose a valid buddy profile.');
  const p={...DEFAULT_PROFILE,...input};
  if(typeof p.name!=='string')throw Error('Use a name with 1–16 letters, numbers, spaces, - or _.');
  p.name=p.name.trim();
  if(!/^[A-Za-z0-9 _-]{1,16}$/.test(p.name))throw Error('Use a name with 1–16 letters, numbers, spaces, - or _.');
  for(const field of ['body','accent','eye'])if(typeof p[field]!=='string'||!/^#[a-f0-9]{6}$/i.test(p[field]))throw Error('Colors must use six hexadecimal digits.');
  for(const [field,choices] of Object.entries(OPTIONS))if(!Number.isInteger(p[field])||p[field]<0||p[field]>=choices.length)throw Error(`Unknown ${field} style.`);
  return Object.fromEntries(Object.keys(DEFAULT_PROFILE).map(k=>[k,p[k]]));
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
  return `DCAV2 ${p.base} ${hex(p.body)} ${hex(p.accent)} ${p.eyes} ${hex(p.eye)} ${p.headwear} ${p.eyewear} ${p.neckwear} ${p.name}`;
}
export function parseAvatarLine(text){
  if(typeof text!=='string'||text.length>128)throw Error('That avatar file is too large or invalid.');
  const line=text.trim();
  let m=/^DCAV2 ([0-4]) ([a-f0-9]{6}) ([a-f0-9]{6}) ([0-5]) ([a-f0-9]{6}) ([0-5]) ([0-3]) ([0-3]) ([A-Za-z0-9 _-]{1,16})$/i.exec(line);
  if(m)return validateProfile({base:+m[1],body:'#'+m[2],accent:'#'+m[3],eyes:+m[4],eye:'#'+m[5],headwear:+m[6],eyewear:+m[7],neckwear:+m[8],name:m[9]});
  m=/^DCAV1 ([0-2]) ([a-f0-9]{6}) ([a-f0-9]{6}) ([0-3]) ([A-Za-z0-9 _-]{1,16})$/i.exec(line);
  if(m)return validateProfile({base:+m[1],body:'#'+m[2],accent:'#'+m[3],eye:+m[1]===2?'#84f7b5':'#101029',headwear:+m[4]===1?1:+m[4]===3?2:0,neckwear:+m[4]===2?1:0,name:m[5]});
  throw Error('Choose a valid Dash-Chan DCAV1 or DCAV2 avatar.cfg file.');
}

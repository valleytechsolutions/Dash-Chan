import assert from 'node:assert/strict';
import {DEFAULT_PROFILE,PRESETS,avatarLine,parseAvatarLine,validateProfile} from '../site/profile.mjs';
for(const p of PRESETS)assert.deepEqual(parseAvatarLine(avatarLine(p)),validateProfile({...p,body:p.body.toUpperCase(),accent:p.accent.toUpperCase(),eye:p.eye.toUpperCase()}));
assert.equal(new Set(PRESETS.map(p=>p.name)).size,PRESETS.length);
for(let base=0;base<5;base++)for(let eyes=0;eyes<6;eyes++)for(let headwear=0;headwear<6;headwear++)for(let eyewear=0;eyewear<4;eyewear++)for(let neckwear=0;neckwear<4;neckwear++){
  const p={...DEFAULT_PROFILE,base,eyes,headwear,eyewear,neckwear};
  const q=parseAvatarLine(avatarLine(p));
  for(const field of ['base','eyes','headwear','eyewear','neckwear','name'])assert.equal(q[field],p[field]);
}
const old=parseAvatarLine('DCAV1 2 112233 FFFFFF 2 Old Orbit\n');
assert.equal(old.neckwear,1);assert.equal(old.headwear,0);assert.equal(old.eye,'#84f7b5');
assert.equal(parseAvatarLine('DCAV1 0 112233 FFFFFF 3 Old Crown').headwear,2);
for(const input of ['<svg onload=alert(1)>','DCAV2 0 FF ZZ 1 0 0 0 0 Hi','DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Good\nSTART','x'.repeat(129)])assert.throws(()=>parseAvatarLine(input));
for(const update of [{eyes:6},{eyes:-1},{eyewear:4},{neckwear:4},{headwear:6},{eye:'#fff'},{name:'<script>'},{name:' '},{base:NaN},{base:5}])assert.throws(()=>avatarLine({...DEFAULT_PROFILE,...update}));
assert.throws(()=>parseAvatarLine('DCAV1 3 000000 FFFFFF 0 Nope'));
console.log('Studio profiles: 2,880 outfit combinations, legacy migration, invalid imports and range limits passed.');

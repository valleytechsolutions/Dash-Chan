import {drawPet} from './pet.mjs';
import {DEFAULT_PROFILE,OPTIONS,PRESETS,avatarLine,parseAvatarLine,validateProfile} from './profile.mjs';

export function initStudio({upload}) {
  const $=id=>document.getElementById(id),form=$('avatar-form');
  const draftKey='dashchan-studio-v2';
  let mood=0,field=0,world=0,lastFrame=0,heroMood=1,heroUntil=0,heroWorld=0;
  const eyeGlyphs=['● ●','✦ ✦','– –','◉ ◉','▪ ▪','● ⌣'];
  function notice(message,error=false){$('avatar-status').textContent=message;$('avatar-status').className=error?'error':'';}
  function download(data,name,type){
    const url=URL.createObjectURL(data instanceof Blob?data:new Blob([data],{type}));
    const a=document.createElement('a');a.href=url;a.download=name;a.click();
    setTimeout(()=>URL.revokeObjectURL(url),30000);
  }
  for(const key of ['base','eyes']) {
    OPTIONS[key].forEach((name,index)=>{
      const label=document.createElement('label');label.className='choice';
      const input=document.createElement('input');input.type='radio';input.name=key;input.value=index;input.checked=index===0;
      const span=document.createElement('span');
      if(key==='eyes'){const glyph=document.createElement('b');glyph.textContent=eyeGlyphs[index];glyph.setAttribute('aria-hidden','true');span.append(glyph);}
      const title=document.createElement('small');title.textContent=name;span.append(title);
      label.append(input,span);$('choices-'+key).append(label);
    });
  }
  for(const key of ['headwear','eyewear','neckwear']) {
    OPTIONS[key].forEach((name,index)=>{const option=document.createElement('option');option.value=index;option.textContent=name;form.elements[key].append(option);});
  }
  function profile(){
    const data=new FormData(form),p={};
    for(const key of Object.keys(DEFAULT_PROFILE))p[key]=Object.hasOwn(OPTIONS,key)?Number(data.get(key)):String(data.get(key));
    return p;
  }
  function saveDraft(){
    $('preview-name').textContent=profile().name.trim()||'Your buddy';
    try{localStorage.setItem(draftKey,JSON.stringify(validateProfile(profile())));$('draft-status').textContent='Draft saved on this browser';}
    catch(e){$('draft-status').textContent=form.checkValidity()?'Draft storage unavailable · download to keep it':'Finish the name to save this draft';}
  }
  function apply(input){
    const p=validateProfile(input);
    for(const [key,value] of Object.entries(p))form.elements[key].value=String(value);
    saveDraft();
  }
  function setMood(value){mood=value;document.querySelectorAll('[data-mood]').forEach(b=>{const active=Number(b.dataset.mood)===mood;b.classList.toggle('selected',active);b.setAttribute('aria-pressed',String(active));});}
  for(const preset of PRESETS){
    const button=document.createElement('button');button.type='button';button.className='preset';button.setAttribute('aria-label','Use '+preset.name+' preset');
    const canvas=document.createElement('canvas');canvas.width=164;canvas.height=140;canvas.setAttribute('aria-hidden','true');
    drawPet(canvas,{...preset,custom:true,field:1},600);
    const label=document.createElement('span');label.textContent=preset.name;button.append(canvas,label);
    button.addEventListener('click',()=>{apply(preset);notice('Starting outfit loaded. Make it your own.');});$('preset-gallery').append(button);
  }
  try{const saved=localStorage.getItem(draftKey);if(saved)apply(validateProfile(JSON.parse(saved)));else apply(DEFAULT_PROFILE);}
  catch{apply(DEFAULT_PROFILE);}
  form.addEventListener('input',saveDraft);
  document.querySelectorAll('[data-mood]').forEach(b=>b.addEventListener('click',()=>setMood(Number(b.dataset.mood))));
  $('avatar-preview').addEventListener('click',()=>setMood(3));
  $('avatar-preview').addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();setMood(3);}});
  $('preview-world').addEventListener('change',e=>{world=Number(e.target.value);});
  $('preview-scene').addEventListener('change',e=>{
    field=Number(e.target.value);
    $('scene-caption').textContent=['Tiny paws. Big curiosity.','Antenna up. Snack radar online.','You drive. I’ll watch the invisible stuff.'][field];
  });
  $('reset-avatar').addEventListener('click',()=>{apply(DEFAULT_PROFILE);notice('Fresh canvas. Your uploaded buddy is unchanged.');});
  $('random-avatar').addEventListener('click',()=>{
    const p={...PRESETS[Math.floor(Math.random()*PRESETS.length)],name:/^[A-Za-z0-9 _-]{1,16}$/.test(profile().name.trim())?profile().name.trim():'My buddy'};
    for(const key of ['eyes','headwear','eyewear','neckwear'])p[key]=Math.floor(Math.random()*OPTIONS[key].length);
    apply(p);notice('A little surprise. Change anything you like.');
  });
  form.addEventListener('submit',e=>{
    e.preventDefault();
    try{download(avatarLine(profile())+'\n','avatar.cfg','text/plain');notice('Saved! Use firmware 0.4+ and copy avatar.cfg to /dashchan/ on SD. That file selects your buddy on every boot.');}
    catch(error){notice(error.message,true);}
  });
  $('import-avatar').addEventListener('change',async e=>{
    const file=e.target.files[0];if(!file)return;
    try{if(file.size>128)throw Error('Choose a small avatar.cfg profile, not a picture or firmware file.');const p=parseAvatarLine(await file.text());apply(p);notice('Buddy imported. You can keep editing or send it to your robot.');}
    catch(error){notice(error.message,true);}finally{e.target.value='';}
  });
  $('export-art').addEventListener('click',()=>{
    const canvas=document.createElement('canvas');canvas.width=880;canvas.height=660;
    drawPet(canvas,{...profile(),custom:true,mood,field},600);
    canvas.toBlob(blob=>{if(blob){download(blob,'dash-chan-portrait.png');notice('Portrait saved with a transparent background. Use avatar.cfg to install the appearance.');}},'image/png');
  });
  $('upload-avatar').addEventListener('click',async()=>{
    if(!form.reportValidity())return;
    try{await upload(avatarLine(profile()),message=>notice(message));notice('Your new buddy is on StackChan! Appearance saved; audio and detection preferences are preserved. An SD avatar.cfg takes priority on boot.');}
    catch(error){notice(error.name==='NotFoundError'?'Port selection canceled.':error.message,error.name!=='NotFoundError');}
  });
  const reduced=matchMedia('(prefers-reduced-motion: reduce)');
  const heroLines=['You drive. I will keep watch.','i Love Your Face','Side quest: invisible snacks!','Discovery dance! Tiny paws up!'];
  document.querySelectorAll('[data-demo]').forEach(button=>button.addEventListener('click',()=>{
    const action=Number(button.dataset.demo);heroMood=action===1?3:action===3?5:1;heroUntil=performance.now()+3200;
    if(action===2)heroWorld=(heroWorld+1)%4;
    $('hero-line').textContent=heroLines[action];
  }));
  drawPet($('hero-pet'),{field:2,mood:1},600);
  function animate(now){
    if(!document.hidden&&now-lastFrame>70){
      lastFrame=now;const time=reduced.matches?600:now;if(heroUntil&&now>heroUntil){heroMood=1;heroUntil=0;$('hero-line').textContent=heroLines[0];}
      $('hero-world').dataset.world=String(heroWorld);
      for(const [id,p] of [['hero-pet',{field:2,mood:heroMood}],['avatar-preview',{...profile(),custom:true,mood,field,world}]]){
        const canvas=$(id),rect=canvas.getBoundingClientRect();
        if(rect.bottom>0&&rect.top<innerHeight)drawPet(canvas,p,time);
      }
    }
    requestAnimationFrame(animate);
  }
  requestAnimationFrame(animate);
}

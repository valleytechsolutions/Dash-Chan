import {drawPet} from './pet.mjs';
import {DEFAULT_PROFILE,OPTIONS,PACKS,PRESETS,PERSONA_LINES,WORLDS,avatarLine,parseAvatarLine,validateProfile,profileSchema,applyPack,randomProfile,shufflePack,contrastIssue} from './profile.mjs';

// How each picker frames the buddy, so a tile shows the part that option changes.
// Face pickers also take the glasses off, or they would hide the very thing being chosen.
const FOCUS={base:{},eyes:{zoom:1.9,focusY:-4,eyewear:0},mark:{zoom:1.7,focusY:2,eyewear:0},headwear:{zoom:1.25,focusY:-20},eyewear:{zoom:1.6,focusY:-8},neckwear:{zoom:1.5,focusY:24},back:{zoom:.98},held:{zoom:1.35,focusX:26,focusY:12},aura:{zoom:.98}};
const TILE_SLOTS=Object.keys(FOCUS);
const HISTORY=20,SHELF=12,FLASH_MS=3200;

export function initStudio({upload}) {
  const $=id=>document.getElementById(id),form=$('avatar-form');
  const draftKey='dashchan-studio-v2',shelfKey='dashchan-shelf-v1';
  let current=validateProfile(DEFAULT_PROFILE),past=[],future=[],lastKey='',lastAt=0;
  let resting=0,flash=null,field=0,world=0,lastFrame=0,tilesDirty=true,heroMood=1,heroUntil=0,heroWorld=0;
  let shelf=[];
  const el=(tag,props={},...children)=>{const node=Object.assign(document.createElement(tag),props);node.append(...children);return node;};
  function notice(message,error=false){$('avatar-status').textContent=message;$('avatar-status').className=error?'error':'';}
  function download(data,name,type){
    const url=URL.createObjectURL(data instanceof Blob?data:new Blob([data],{type}));
    const a=document.createElement('a');a.href=url;a.download=name;a.click();
    setTimeout(()=>URL.revokeObjectURL(url),30000);
  }
  const thumb=(w=160,h=120)=>{const c=el('canvas',{width:w,height:h});c.setAttribute('aria-hidden','true');return c;};

  // Pickers: one radio per option, drawn on the user's own buddy. Radios keep the form's
  // field names, native keyboard behaviour and labels.
  const tiles=[];
  for(const key of TILE_SLOTS){
    OPTIONS[key].forEach((name,index)=>{
      const input=el('input',{type:'radio',name:key,value:String(index),checked:index===0});
      const canvas=thumb();
      tiles.push({key,index,canvas,panel:$('choices-'+key).closest('.tab-panel')});
      $('choices-'+key).append(el('label',{className:'tile'},input,el('span',{className:'tile-face'},canvas,el('small',{},name))));
    });
  }
  OPTIONS.persona.forEach((name,index)=>{
    const input=el('input',{type:'radio',name:'persona',value:String(index),checked:index===0});
    $('choices-persona').append(el('label',{className:'choice'},input,el('span',{},el('small',{},name))));
  });

  // Reads the form. The name is returned as typed; everything else is always valid.
  function read(){
    const data=new FormData(form),p={};
    for(const key of Object.keys(DEFAULT_PROFILE))p[key]=Object.hasOwn(OPTIONS,key)?Number(data.get(key)):String(data.get(key));
    return p;
  }
  const nameOk=name=>/^[A-Za-z0-9 _-]{1,16}$/.test(name.trim());
  // The design as it stands, with the last good name if the name box is mid-edit.
  const design=()=>{const p=read();return validateProfile(nameOk(p.name)?p:{...p,name:current.name});};
  function write(p){for(const [key,value] of Object.entries(p))form.elements[key].value=String(value);}
  function saveDraft(){
    try{localStorage.setItem(draftKey,JSON.stringify(current));$('draft-status').textContent='Draft saved on this browser';}
    catch{$('draft-status').textContent='Draft storage unavailable · download to keep it';}
  }
  function refresh(){
    const p=read(),valid=nameOk(p.name);
    $('preview-name').textContent=p.name.trim()||'Your buddy';
    if(!valid)$('draft-status').textContent='Finish the name to save this draft';
    $('undo-avatar').disabled=!past.length;$('redo-avatar').disabled=!future.length;
    for(const button of $('pack-strip').children){button.setAttribute('aria-pressed',String(Number(button.dataset.pack)===p.pack));}
    $('shuffle-pack').title=p.pack?`New colours and gear from the ${PACKS[p.pack].name} pack`:'Picks a style pack, then mixes its colours and gear';
    const issue=contrastIssue({...p,name:'x'});
    $('contrast-warning').hidden=!issue;$('tab-colours').classList.toggle('warn',!!issue);
    if(issue){$('contrast-text').textContent=issue.message;$('contrast-fix').dataset.fix=issue.fix;}
    const speaker=p.persona||p.base+1;
    $('persona-sample').textContent=`“${PERSONA_LINES[speaker]}” · ${p.persona?'speaking as '+OPTIONS.persona[p.persona]:'its own voice, as '+OPTIONS.base[p.base].split(' ')[0]}`;
    const schema=profileSchema({...p,name:'x'});
    $('format-note').textContent=schema===2?'Classic design: saves as DCAV2 and loads on Dash-Chan 0.3 or newer (Wisp and Ember need 0.4).':'Uses the new gear: saves as DCAV3, which needs up-to-date Dash-Chan firmware. Older firmware refuses it safely and keeps its current buddy.';
    tilesDirty=true;
  }
  // Records a change. Quick repeats on one control (a colour drag, typing) share an undo step.
  function commit(p,key=''){
    if(JSON.stringify(p)===JSON.stringify(current)){refresh();return;}
    const now=performance.now();
    if(!(key&&key===lastKey&&now-lastAt<900)){past.push(current);if(past.length>HISTORY)past.shift();}
    future=[];lastKey=key;lastAt=now;current=p;saveDraft();refresh();
  }
  function apply(input,key=''){const p=validateProfile(input);write(p);commit(p,key);}
  // For actions that restyle the buddy but promise to leave its name alone: a name that is
  // still being typed (and is not valid yet) stays in the box exactly as typed.
  function restyle(make,key=''){
    const typed=form.elements.name.value;apply(make(design()),key);
    if(!nameOk(typed)){form.elements.name.value=typed;refresh();}
  }
  function step(from,to){
    if(!from.length)return;
    to.push(current);current=from.pop();lastKey='';write(current);saveDraft();refresh();
    notice(from===past?'Undone.':'Redone.');
  }

  // Style packs: the current shape in each pack's signature look.
  PACKS.forEach((style,id)=>{
    if(!style)return;
    const button=el('button',{type:'button',className:'pack'},thumb(200,150),el('strong',{},style.name),el('small',{},style.blurb));
    button.dataset.pack=String(id);button.setAttribute('aria-pressed','false');
    button.addEventListener('click',()=>{
      restyle(p=>applyPack(p,id));world=style.world;$('preview-world').value=String(world);
      notice(`${style.name} look applied. Name, shape and persona are unchanged; the preview world is now ${WORLDS[world]}.`);
    });
    $('pack-strip').append(button);
  });
  // Palettes: each pack's colours plus the five classic buddies.
  const palettes=[...PACKS.flatMap(style=>style?style.palettes.map((tones,i)=>({...tones,label:`${style.name} ${i+1}`})):[]),
    {body:'#fff3d2',accent:'#ff88b6',eye:'#101029',label:'Mochi cream'},{body:'#a5edc0',accent:'#ff9dc8',eye:'#245549',label:'Sprout mint'},
    {body:'#c9c2ff',accent:'#7af3ce',eye:'#9affdd',label:'Orbit lilac'},{body:'#000000',accent:'#c5bfff',eye:'#edffff',label:'Wisp night'},{body:'#ff4d24',accent:'#ffe040',eye:'#201029',label:'Ember glow'}];
  for(const tones of palettes){
    const button=el('button',{type:'button',className:'swatch'},...['body','accent','eye'].map(k=>{const dot=el('i');dot.style.background=tones[k];return dot;}));
    button.setAttribute('aria-label',`${tones.label} palette`);button.title=tones.label;
    button.addEventListener('click',()=>{restyle(p=>({...p,body:tones.body,accent:tones.accent,eye:tones.eye}));notice(`${tones.label} colours applied.`);});
    $('palette-swatches').append(button);
  }
  // Starters and the shelf of saved designs.
  for(const preset of PRESETS){
    const canvas=thumb(164,123),button=el('button',{type:'button',className:'preset'},canvas,el('span',{},preset.name));
    button.setAttribute('aria-label','Use '+preset.name+' preset');
    drawPet(canvas,{...preset,custom:true,still:true},600);
    button.addEventListener('click',()=>{restyle(p=>({...preset,name:p.name}));notice('Starter loaded. Your buddy keeps its name.');});
    $('preset-gallery').append(button);
  }
  function saveShelf(){try{localStorage.setItem(shelfKey,JSON.stringify(shelf));return true;}catch{return false;}}
  function drawShelf(){
    const box=$('shelf');box.replaceChildren();
    if(!shelf.length)box.append(el('p',{className:'micro shelf-empty'},'Nothing saved yet. Designs you save appear here, ready to load or send.'));
    shelf.forEach((saved,index)=>{
      const canvas=thumb(164,123),load=el('button',{type:'button',className:'preset'},canvas,el('span',{},saved.name));
      load.setAttribute('aria-label','Load '+saved.name);
      drawPet(canvas,{...saved,custom:true,still:true},600);
      load.addEventListener('click',()=>{apply(saved);notice(`${saved.name} loaded from My buddies.`);});
      const remove=el('button',{type:'button',className:'shelf-remove'},'×');
      remove.setAttribute('aria-label',`Remove ${saved.name} from My buddies`);
      remove.addEventListener('click',()=>{shelf.splice(index,1);saveShelf();drawShelf();notice(`${saved.name} removed from My buddies. Your current design is unchanged.`);});
      box.append(el('div',{className:'shelf-item'},load,remove));
    });
  }
  $('shelf-save').addEventListener('click',()=>{
    if(!form.reportValidity())return;
    // Save what is on screen or nothing: never the design under the last good name instead.
    let p;try{p=validateProfile(read());}catch(error){notice(error.message,true);return;}
    const line=avatarLine(p),at=shelf.findIndex(s=>avatarLine(s)===line);
    if(at>=0){notice(`${p.name} is already in My buddies.`);return;}
    if(shelf.length>=SHELF){notice(`My buddies holds ${SHELF} designs. Remove one to make room.`,true);return;}
    shelf.unshift(p);drawShelf();const stored=saveShelf();
    notice(stored?`${p.name} saved to My buddies on this browser.`:'This browser would not store the design. Download avatar.cfg to keep it.',!stored);
  });

  // Tabs. Arrow keys move between them, as in any tab list.
  const tabs=[...document.querySelectorAll('.studio-tabs [role=tab]')];
  function showTab(tab,focus=false){
    for(const t of tabs){const on=t===tab;t.setAttribute('aria-selected',String(on));t.tabIndex=on?0:-1;$(t.getAttribute('aria-controls')).hidden=!on;}
    if(focus)tab.focus();
    tilesDirty=true;
  }
  tabs.forEach((tab,i)=>{
    tab.addEventListener('click',()=>showTab(tab));
    tab.addEventListener('keydown',e=>{
      const to=e.key==='ArrowRight'?tabs[(i+1)%tabs.length]:e.key==='ArrowLeft'?tabs[(i+tabs.length-1)%tabs.length]:e.key==='Home'?tabs[0]:e.key==='End'?tabs.at(-1):null;
      if(to){e.preventDefault();showTab(to,true);}
    });
  });

  // Expressions. Love and Cheer are a quick look: they fade back to the resting face.
  const shown=()=>flash?flash.mood:resting;
  function paintMood(){document.querySelectorAll('[data-mood]').forEach(b=>{const active=Number(b.dataset.mood)===shown();b.classList.toggle('selected',active);b.setAttribute('aria-pressed',String(active));});}
  function setMood(value){
    if(value===3||value===5)flash={mood:value,until:performance.now()+FLASH_MS};
    else{resting=value;flash=null;}
    paintMood();
  }

  try{const saved=localStorage.getItem(draftKey);current=validateProfile(saved?JSON.parse(saved):DEFAULT_PROFILE);}
  catch{current=validateProfile(DEFAULT_PROFILE);}
  try{const saved=JSON.parse(localStorage.getItem(shelfKey)||'[]');if(Array.isArray(saved))for(const item of saved.slice(0,SHELF)){try{shelf.push(validateProfile(item));}catch{}}}
  catch{}
  write(current);saveDraft();refresh();drawShelf();
  const millions=Math.floor(TILE_SLOTS.reduce((n,key)=>n*OPTIONS[key].length,1)/1e6);
  $('look-count').textContent=`over ${millions} million`;
  // On a phone the preview is pinned, so its extras start folded away to leave room to edit.
  const phone=matchMedia('(max-width:680px)'),fold=()=>{if(phone.matches)$('preview-more').open=false;};
  fold();phone.addEventListener('change',fold);

  form.addEventListener('input',e=>{
    const p=read();
    // Typing and colour drags share one undo step; each picked tile is a step of its own.
    if(nameOk(p.name))commit(validateProfile(p),e.target.type==='radio'?'':e.target.name||'');
    else refresh();
  });
  document.querySelectorAll('[data-mood]').forEach(b=>b.addEventListener('click',()=>setMood(Number(b.dataset.mood))));
  $('avatar-preview').addEventListener('click',()=>setMood(3));
  $('avatar-preview').addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();setMood(3);}});
  $('preview-world').addEventListener('change',e=>{world=Number(e.target.value);});
  $('preview-scene').addEventListener('change',e=>{
    field=Number(e.target.value);
    $('scene-caption').textContent=['Tiny paws. Big curiosity.','Antenna up. Snack radar online.','You drive. I’ll watch the invisible stuff.'][field];
  });
  $('undo-avatar').addEventListener('click',()=>step(past,future));
  $('redo-avatar').addEventListener('click',()=>step(future,past));
  document.addEventListener('keydown',e=>{
    if(!(e.ctrlKey||e.metaKey)||$('creator').hidden||e.target.matches?.('input:not([type=radio]):not([type=color]),textarea,select'))return;
    const key=e.key.toLowerCase();
    if(key==='z'&&!e.shiftKey){e.preventDefault();step(past,future);}
    else if(key==='y'||(key==='z'&&e.shiftKey)){e.preventDefault();step(future,past);}
  });
  $('reset-avatar').addEventListener('click',()=>{apply(DEFAULT_PROFILE);notice('Fresh canvas. Undo brings the last design back; your uploaded buddy is unchanged.');});
  $('random-avatar').addEventListener('click',()=>{restyle(p=>randomProfile(Math.random,p));notice('A whole new buddy, same name. Undo if you liked the last one better.');});
  $('shuffle-pack').addEventListener('click',()=>{
    let p;restyle(now=>p=shufflePack(Math.random,now));world=PACKS[p.pack].world;$('preview-world').value=String(world);
    notice(`Shuffled within ${PACKS[p.pack].name}. Shape and name stay put.`);
  });
  $('contrast-fix').addEventListener('click',e=>{const eye=e.currentTarget.dataset.fix;restyle(p=>({...p,eye}));notice('Eye colour changed so the face reads clearly.');});
  // Enter in the name box means "done typing", not "download a file": without this the form's
  // only submit button (Download avatar.cfg) would fire.
  form.elements.name.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();e.target.blur();}});
  form.addEventListener('submit',e=>{
    e.preventDefault();
    try{
      const line=avatarLine(read());download(line+'\n','avatar.cfg','text/plain');
      notice(line.startsWith('DCAV3')?'Saved as DCAV3. Copy avatar.cfg to /dashchan/ on SD; it needs up-to-date Dash-Chan firmware and selects your buddy on every boot.':'Saved! Use firmware 0.4+ and copy avatar.cfg to /dashchan/ on SD. That file selects your buddy on every boot.');
    }catch(error){notice(error.message,true);}
  });
  $('import-avatar').addEventListener('change',async e=>{
    const file=e.target.files[0];if(!file)return;
    try{if(file.size>128)throw Error('Choose a small avatar.cfg profile, not a picture or firmware file.');const p=parseAvatarLine(await file.text());apply(p);notice('Buddy imported. You can keep editing or send it to your robot.');}
    catch(error){notice(error.message,true);}finally{e.target.value='';}
  });
  $('export-art').addEventListener('click',()=>{
    const canvas=document.createElement('canvas');canvas.width=880;canvas.height=660;
    drawPet(canvas,{...read(),custom:true,mood:shown(),field},600);
    canvas.toBlob(blob=>{if(blob){download(blob,'dash-chan-portrait.png');notice('Portrait saved with a transparent background. Use avatar.cfg to install the appearance.');}},'image/png');
  });
  $('upload-avatar').addEventListener('click',async()=>{
    if(!form.reportValidity())return;
    try{await upload(avatarLine(read()),message=>notice(message));notice('Your new buddy is on StackChan! Appearance saved; audio and detection preferences are preserved. An SD avatar.cfg takes priority on boot.');}
    catch(error){notice(error.name==='NotFoundError'?'Port selection canceled.':error.message,error.name!=='NotFoundError');}
  });

  const reduced=matchMedia('(prefers-reduced-motion: reduce)');
  const heroLines=['You drive. I will keep watch.','Hehe. That is the good spot.','Side quest: invisible snacks!','Discovery dance! Tiny paws up!'];
  document.querySelectorAll('[data-demo]').forEach(button=>button.addEventListener('click',()=>{
    const action=Number(button.dataset.demo);heroMood=action===1?3:action===3?5:1;heroUntil=performance.now()+3200;
    if(action===2)heroWorld=(heroWorld+1)%4;
    $('hero-line').textContent=heroLines[action];
  }));
  drawPet($('hero-pet'),{field:2,mood:1},600);
  function drawTiles(){
    const p={...read(),custom:true,still:true};
    // The aura is left off the other pickers so each tile shows its own option clearly.
    for(const tile of tiles)if(!tile.panel.hidden)drawPet(tile.canvas,{...p,aura:0,...FOCUS[tile.key],[tile.key]:tile.index},600);
    for(const button of $('pack-strip').children)drawPet(button.firstChild,{...p,...PACKS[Number(button.dataset.pack)].look},600);
  }
  function animate(now){
    if(flash&&now>flash.until){flash=null;paintMood();}
    if(!document.hidden&&now-lastFrame>70){
      lastFrame=now;const time=reduced.matches?600:now;if(heroUntil&&now>heroUntil){heroMood=1;heroUntil=0;$('hero-line').textContent=heroLines[0];}
      $('hero-world').dataset.world=String(heroWorld);
      for(const [id,p] of [['hero-pet',{field:2,mood:heroMood}],['avatar-preview',{...read(),custom:true,mood:shown(),field,world}]]){
        const canvas=$(id),rect=canvas.getBoundingClientRect();
        if(rect.bottom>0&&rect.top<innerHeight)drawPet(canvas,p,time);
      }
      // Thumbnails are still pictures: redraw them only when the design or the open tab changed.
      if(tilesDirty&&!$('creator').hidden){tilesDirty=false;drawTiles();}
    }
    requestAnimationFrame(animate);
  }
  requestAnimationFrame(animate);
}

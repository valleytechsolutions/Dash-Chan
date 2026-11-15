import {createRequire} from 'node:module';
import assert from 'node:assert/strict';
import {mkdir,readFile} from 'node:fs/promises';
const {chromium}=createRequire(import.meta.url)('playwright');
const url=process.env.DASH_SITE_URL||'http://127.0.0.1:8765/';
for(let attempt=0;;attempt++){
  try{const r=await fetch(url,{signal:AbortSignal.timeout(2000)});await r.arrayBuffer();if(!r.ok)throw Error(`HTTP ${r.status}`);break;}
  catch(e){if(attempt>=19)throw e;await new Promise(resolve=>setTimeout(resolve,250));}
}
await mkdir('build/web',{recursive:true});
const browser=await chromium.launch({headless:true,...(process.env.DASH_BROWSER?{executablePath:process.env.DASH_BROWSER}:{})});
try{
  const errors=[],page=await browser.newPage({viewport:{width:1440,height:1000}});
  page.on('pageerror',e=>errors.push(e.message));
  page.on('response',r=>{if(r.status()>=400)errors.push(`${r.status()} ${r.url()}`);});
  await page.goto(url);await page.locator('#flash-status').filter({hasText:'Ready.'}).waitFor();
  assert.equal(await page.locator('#connect').isDisabled(),true);
  await page.locator('#hardware-confirm').check();assert.equal(await page.locator('#connect').isDisabled(),false);
  await page.locator('[data-demo="1"]').click();assert.match(await page.locator('#hero-line').textContent(),/good spot/);
  await page.locator('[data-demo="2"]').click();
  await page.locator('.tabs a[href="#creator"]').click();
  const tab=name=>page.getByRole('tab',{name,exact:true}),pick=name=>page.getByRole('radio',{name,exact:true});
  const chosen=slot=>page.locator(`input[name=${slot}]:checked`).inputValue();
  const saved=async file=>{const d=page.waitForEvent('download');await page.locator('#save-avatar').click();await (await d).saveAs(`build/web/${file}`);return readFile(`build/web/${file}`,'utf8');};
  const packButton=name=>page.locator('#pack-strip').getByRole('button',{name:new RegExp('^'+name)});
  const nameBox=page.locator('input[name=name]');
  await nameBox.fill('Captain Mochi');
  // Pickers are tabbed tiles, each a labelled radio that keeps the old form field name.
  assert.equal(await tab('Shape').getAttribute('aria-selected'),'true');
  await tab('Face').click();assert.equal(await tab('Face').getAttribute('aria-selected'),'true');assert.equal(await page.locator('#panel-shape').isHidden(),true);
  await pick('Sparkle').check();
  await tab('Outfit').click();
  await pick('Scout cap').check();await pick('Round glasses').check();await pick('Bandana').check();
  for(const [slot,count] of [['headwear',12],['eyewear',8],['neckwear',8],['back',7],['held',6]])assert.equal(await page.locator(`#choices-${slot} canvas`).count(),count);
  await page.locator('#preview-scene').selectOption('2');
  for(const world of ['0','1','2','3'])await page.locator('#preview-world').selectOption(world);
  await page.getByRole('button',{name:'Cheer',exact:true}).click();assert.equal(await page.getByRole('button',{name:'Cheer',exact:true}).getAttribute('aria-pressed'),'true');
  await page.getByRole('button',{name:'Alert',exact:true}).click();
  // A design without new gear still saves as DCAV2, so it loads on older firmware.
  assert.equal(await saved('avatar.cfg'),'DCAV2 0 FFF3D2 FF88B6 1 101029 5 1 2 Captain Mochi\n');
  assert.match(await page.locator('#format-note').textContent(),/DCAV2/);
  // A draft survives reload. No USB operation is needed to edit or save.
  await page.reload();await page.locator('#draft-status').filter({hasText:'Draft saved'}).waitFor();
  assert.equal(await nameBox.inputValue(),'Captain Mochi');
  assert.equal(await chosen('headwear'),'5');assert.equal(await chosen('eyes'),'1');
  await page.locator('#import-avatar').setInputFiles({name:'avatar.cfg',mimeType:'text/plain',buffer:Buffer.from('DCAV1 2 112233 FFAABB 2 Old Orbit\n')});
  await page.locator('#avatar-status').filter({hasText:'Buddy imported'}).waitFor();
  assert.equal(await nameBox.inputValue(),'Old Orbit');
  assert.equal(await chosen('neckwear'),'1');assert.equal(await chosen('base'),'2');
  await page.locator('#import-avatar').setInputFiles({name:'bad.cfg',mimeType:'text/plain',buffer:Buffer.from('<script>not an avatar</script>')});
  await page.locator('#avatar-status.error').waitFor();
  assert.equal(await nameBox.inputValue(),'Old Orbit');
  // Starters, packs, Surprise me and Shuffle restyle the buddy; none of them renames it.
  await page.getByRole('button',{name:'Use Signal scout preset',exact:true}).click();
  assert.equal(await nameBox.inputValue(),'Old Orbit');assert.equal(await chosen('headwear'),'5');assert.equal(await chosen('base'),'0');
  await page.locator('#preview-scene').selectOption('2');
  await page.getByRole('button',{name:'Happy',exact:true}).click();
  await page.locator('#avatar-preview').scrollIntoViewIfNeeded();
  await page.locator('#creator').screenshot({path:'build/web/studio-desktop.png'});
  const art=page.waitForEvent('download');await page.locator('#export-art').click();
  await (await art).saveAs('build/web/portrait.png');
  const png=await readFile('build/web/portrait.png');assert.equal(png.readUInt32BE(16),880);assert.equal(png.readUInt32BE(20),660);assert.equal(png[25],6);
  // Petting is a quick look: heart eyes now, the resting face again a few seconds later.
  await page.locator('#avatar-preview').focus();await page.keyboard.press('Enter');
  assert.equal(await page.getByRole('button',{name:'Love',exact:true}).getAttribute('aria-pressed'),'true');
  await page.locator('[data-mood="0"][aria-pressed=true]').waitFor({timeout:6000});
  assert.equal(await page.getByRole('button',{name:'Love',exact:true}).getAttribute('aria-pressed'),'false');
  await nameBox.fill('Bad;name');assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),false);
  // A name that is still being typed stays in the box as typed when the buddy is restyled; the
  // status says the name is unchanged, so the box must not jump back to the last good one.
  const eyesBefore=await page.locator('input[name=body]').inputValue();
  await page.locator('#random-avatar').click();
  assert.equal(await nameBox.inputValue(),'Bad;name');assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),false);
  assert.notEqual(await page.locator('input[name=body]').inputValue(),eyesBefore,'Surprise me still restyles');
  await page.getByRole('button',{name:'Use Petal patrol preset',exact:true}).click();assert.equal(await nameBox.inputValue(),'Bad;name');assert.equal(await chosen('headwear'),'4');
  await packButton('Goth').click();assert.equal(await nameBox.inputValue(),'Bad;name');assert.equal(await packButton('Goth').getAttribute('aria-pressed'),'true');
  assert.match(await page.locator('#draft-status').textContent(),/Finish the name/);
  // Saving to the shelf takes the design on screen or nothing: a blank or invalid name is
  // refused with the name error, never stored under the previous name.
  for(const typed of ['Bad;name','   ']){
    await nameBox.fill(typed);await page.locator('#shelf-save').click();
    assert.equal(await page.locator('#shelf .shelf-item').count(),0,`"${typed}" was shelved`);
    assert.doesNotMatch(await page.locator('#avatar-status').textContent(),/saved to My buddies/);
  }
  assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),false,'An all-space name is not valid');
  // The same refusal without the browser's own field check in the way.
  const pattern=await nameBox.getAttribute('pattern');
  await nameBox.evaluate(e=>{e.removeAttribute('pattern');e.required=false;});await nameBox.fill('   ');await page.locator('#shelf-save').click();
  await page.locator('#avatar-status.error').filter({hasText:'Use a name'}).waitFor();assert.equal(await page.locator('#shelf .shelf-item').count(),0);
  await nameBox.evaluate((e,p)=>{e.setAttribute('pattern',p);e.required=true;},pattern);
  // Enter in the name box finishes typing. It must not submit the form, which would download
  // avatar.cfg.
  await nameBox.fill('Nyx');
  let stray=0;const countDownload=()=>stray++;page.on('download',countDownload);
  await nameBox.press('Enter');await page.waitForTimeout(500);
  assert.equal(stray,0,'Enter in the name box downloaded a file');page.off('download',countDownload);
  assert.equal(await nameBox.evaluate(e=>e===document.activeElement),false);assert.equal(await nameBox.inputValue(),'Nyx');
  assert.doesNotMatch(await page.locator('#avatar-status').textContent(),/Saved/);
  const surprise=await saved('surprise.cfg');assert.match(surprise,/^DCAV[23] [0-4] [0-9A-F]{6} .* Nyx\n$/);
  await page.locator('#random-avatar').click();assert.notEqual(await saved('surprise2.cfg'),surprise);assert.equal(await nameBox.inputValue(),'Nyx');
  assert.equal(await page.locator('#contrast-warning').isHidden(),true,'Surprise me never makes unreadable eyes');
  // Style packs dress the current shape and suggest a world.
  assert.equal(await page.locator('#pack-strip button').count(),5);
  await tab('Vibe').click();await pick('Match shape').check(); // Surprise me picked a persona too; packs keep whatever is set.
  await tab('Shape').click();await pick('Wisp · ghost').check();
  const beforePack=await saved('before-pack.cfg');
  await packButton('Goth').click();
  assert.equal(await packButton('Goth').getAttribute('aria-pressed'),'true');assert.equal(await packButton('Vaporwave').getAttribute('aria-pressed'),'false');
  assert.equal(await nameBox.inputValue(),'Nyx');assert.equal(await page.locator('#preview-world').inputValue(),'2');
  const goth='DCAV3 3 17141F B3123C F2E9FF 9 7 0 5 4 2 4 5 3 0 Nyx\n';
  assert.equal(await saved('goth.cfg'),goth);assert.match(await page.locator('#format-note').textContent(),/DCAV3/);
  await tab('Vibe').click();await pick('Mochi').check();
  assert.match(await page.locator('#persona-sample').textContent(),/router off a shelf/);
  assert.equal(await saved('goth-persona.cfg'),goth.replace(' 3 0 Nyx',' 3 1 Nyx'));
  // Undo and redo, by button and by keyboard.
  await page.locator('#undo-avatar').click();assert.equal(await saved('undo.cfg'),goth);
  await page.keyboard.press('Control+z');assert.equal(await packButton('Goth').getAttribute('aria-pressed'),'false');assert.equal(await saved('undo2.cfg'),beforePack);
  await page.locator('#redo-avatar').click();assert.equal(await saved('redo.cfg'),goth);
  await page.keyboard.press('Control+Shift+Z');assert.equal(await chosen('persona'),'1');assert.equal(await page.locator('#redo-avatar').isDisabled(),true);
  await page.locator('#shuffle-pack').click();
  assert.equal(await packButton('Goth').getAttribute('aria-pressed'),'true');assert.equal(await nameBox.inputValue(),'Nyx');assert.equal(await chosen('base'),'3');
  assert.match(await saved('shuffle.cfg'),/^DCAV3 3 .* 3 1 Nyx\n$/);
  // Tabs work from the keyboard.
  await tab('Vibe').focus();await page.keyboard.press('Home');assert.equal(await tab('Shape').getAttribute('aria-selected'),'true');
  await page.keyboard.press('ArrowRight');await page.keyboard.press('ArrowRight');assert.equal(await tab('Colours').getAttribute('aria-selected'),'true');
  assert.equal(await page.locator('#panel-colours').isVisible(),true);
  // Eyes the same colour as the body get a warning and a one-tap fix.
  await page.getByRole('button',{name:'Mochi cream palette',exact:true}).click();
  assert.equal(await page.locator('#contrast-warning').isHidden(),true);
  await page.locator('input[name=eye]').fill('#fff3d2');
  await page.locator('#contrast-warning').waitFor();assert.match(await page.locator('#contrast-text').textContent(),/vanish|hard to see/);
  await page.locator('#contrast-fix').click();assert.equal(await page.locator('#contrast-warning').isHidden(),true);
  assert.notEqual(await page.locator('input[name=eye]').inputValue(),'#fff3d2');
  // My buddies: a shelf of saved designs that survives a reload.
  await page.locator('#shelf-save').click();await page.locator('#avatar-status').filter({hasText:'saved to My buddies'}).waitFor();
  const shelved=await saved('shelved.cfg');
  await page.locator('#shelf-save').click();await page.locator('#avatar-status').filter({hasText:'already in My buddies'}).waitFor();
  await page.locator('#reset-avatar').click();assert.equal(await nameBox.inputValue(),'My buddy');
  await page.reload();await page.locator('#draft-status').filter({hasText:'Draft saved'}).waitFor();
  assert.equal(await page.locator('#shelf .shelf-item').count(),1);
  await page.getByRole('button',{name:'Load Nyx',exact:true}).click();assert.equal(await saved('reloaded.cfg'),shelved);
  await page.getByRole('button',{name:'Remove Nyx from My buddies',exact:true}).click();assert.equal(await page.locator('#shelf .shelf-item').count(),0);
  assert.equal(await saved('still-here.cfg'),shelved);
  // A DCAV3 file imports with every field.
  await page.locator('#import-avatar').setInputFiles({name:'avatar.cfg',mimeType:'text/plain',buffer:Buffer.from('DCAV3 4 B967FF 01CDFE FFFB96 7 4 0 0 2 1 1 1 4 5 Vapor Ember\n')});
  await page.locator('#avatar-status').filter({hasText:'Buddy imported'}).waitFor();
  assert.equal(await nameBox.inputValue(),'Vapor Ember');assert.equal(await chosen('aura'),'1');assert.equal(await chosen('persona'),'5');
  assert.equal(await packButton('Vaporwave').getAttribute('aria-pressed'),'true');
  assert.equal(await saved('imported.cfg'),'DCAV3 4 B967FF 01CDFE FFFB96 7 4 0 0 2 1 1 1 4 5 Vapor Ember\n');
  // A pack tag alone never forces the newer format: take the pack's gear off again and the
  // design is a classic one that older firmware reads, while the pack card stays selected.
  await page.evaluate(()=>{const form=document.getElementById('avatar-form');
    for(const slot of ['eyes','headwear','eyewear','neckwear','mark','back','held','aura','persona']){const none=form.querySelector(`input[name=${slot}][value="0"]`);none.checked=true;none.dispatchEvent(new Event('input',{bubbles:true}));}});
  assert.equal(await packButton('Vaporwave').getAttribute('aria-pressed'),'true');
  assert.match(await page.locator('#format-note').textContent(),/DCAV2/);
  assert.equal(await saved('undressed.cfg'),'DCAV2 4 B967FF 01CDFE 0 FFFB96 0 0 0 Vapor Ember\n');
  // The help text in the lilac preview panel is readable (WCAG AA for small text is 4.5:1).
  const contrast=await page.locator('.preview .micro').first().evaluate(node=>{
    const rgb=text=>text.match(/\d+(\.\d+)?/g).slice(0,3).map(Number),lum=c=>{const [r,g,b]=c.map(v=>{v/=255;return v<=.03928?v/12.92:((v+.055)/1.055)**2.4;});return .2126*r+.7152*g+.0722*b;};
    let panel=node;while(panel&&getComputedStyle(panel).backgroundColor.match(/, 0\)$|transparent/))panel=panel.parentElement;
    const a=lum(rgb(getComputedStyle(node).color)),b=lum(rgb(getComputedStyle(panel).backgroundColor));return (Math.max(a,b)+.05)/(Math.min(a,b)+.05);});
  assert(contrast>=4.5,`Preview help text contrast is ${contrast.toFixed(2)}:1`);
  await page.getByRole('button',{name:'Happy',exact:true}).click();
  assert.equal(await page.locator('#preset-gallery button').count(),6);
  for(const [name,base] of [['Wisp',3],['Ember',4]]){
    await page.getByRole('button',{name:`Use ${name} preset`,exact:true}).click();
    assert.equal(await nameBox.inputValue(),'Vapor Ember');
    assert.match(await saved(`${name}.cfg`),new RegExp(`^DCAV2 ${base} `));
    await page.locator('#avatar-preview').screenshot({path:`build/web/${name}.png`});
  }
  // A saved name made of the widest letters wraps inside its shelf card on a phone.
  await nameBox.fill('WWWWWWWWWWWWWWWW');await page.locator('#shelf-save').click();await page.locator('#avatar-status').filter({hasText:'saved to My buddies'}).waitFor();
  await page.setViewportSize({width:390,height:844});await page.locator('#shelf .preset').first().scrollIntoViewIfNeeded();
  const wideName=await page.locator('#shelf .preset').first().evaluate(card=>{const c=card.getBoundingClientRect(),t=card.querySelector('span').getBoundingClientRect();return {card:[c.left,c.right],text:[t.left,t.right]};});
  assert(wideName.text[0]>=wideName.card[0]-1&&wideName.text[1]<=wideName.card[1]+1&&wideName.card[0]>=0,`Shelf name overflows its card: ${JSON.stringify(wideName)}`);
  await page.locator('#shelf').screenshot({path:'build/web/shelf-wide-name.png'});
  await page.getByRole('button',{name:'Remove WWWWWWWWWWWWWWWW from My buddies',exact:true}).click();
  await page.setViewportSize({width:1440,height:1000});await nameBox.fill('Vapor Ember');
  const badImages=await page.locator('img').evaluateAll(imgs=>imgs.filter(i=>i.getBoundingClientRect().width&&(!i.complete||!i.naturalWidth)).map(i=>i.src));assert.deepEqual(badImages,[]);
  await page.locator('.tabs a[href="#install"]').click();await page.screenshot({path:'build/web/desktop.png',fullPage:true});
  for(const width of [390,320,768]){
    await page.setViewportSize({width,height:844});await page.locator('.tabs a[href="#creator"]').click();await page.locator('#avatar-preview').scrollIntoViewIfNeeded();
    await page.locator('#creator').screenshot({path:`build/web/studio-${width}.png`});
    if(width<=680){
      // On a phone the preview stays pinned while the pickers scroll under it.
      await page.getByRole('tab',{name:'Outfit',exact:true}).click();await page.locator('#choices-held').scrollIntoViewIfNeeded();
      const box=await page.locator('#avatar-preview').boundingBox();
      assert(box&&box.y>=0&&box.y+box.height<=844*.45,`Preview not pinned at ${width}px: ${JSON.stringify(box)}`);
      assert(await page.locator('#choices-held').isVisible());
      await page.screenshot({path:`build/web/studio-${width}-editing.png`});
      await page.getByRole('tab',{name:'Shape',exact:true}).click();
    }
    const overflow=await page.evaluate(()=>[...document.querySelectorAll('body *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).map(e=>[e.tagName,e.className,Math.round(e.getBoundingClientRect().right)]));
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`Overflow at ${width}px: ${JSON.stringify(overflow)}`);
  }
  await page.locator('.tabs a[href="#uploads"]').click();
  const sample=await readFile('build/test-wigle.csv');
  await page.locator('#drive-file').setInputFiles({name:'synthetic.csv',mimeType:'text/csv',buffer:sample});
  await page.locator('#drive-summary').filter({hasText:'2 observations'}).waitFor();
  assert(await page.locator('#send-drive').isDisabled());
  await page.locator('#browser-wigle-name').fill('synthetic-api');await page.locator('#browser-wigle-token').fill('synthetic-token');
  let requests=0;await page.route('https://api.wigle.net/api/v2/file/upload',async route=>{requests++;await route.fulfill({status:200,contentType:'application/json',body:'{"success":true}'});});
  await page.locator('#share-confirm').check();await page.locator('#send-drive').click();
  await page.locator('#upload-status').filter({hasText:'accepted'}).waitFor();assert.equal(requests,1);assert(await page.locator('#send-drive').isDisabled());
  const form=page.locator('#upload-config-form');await form.locator('[name=ssid]').fill('Synthetic-network');await form.locator('[name=password]').fill('synthetic-password');await form.locator('[name=wigle]').check();await form.locator('[name=wigleName]').fill('synthetic-api');await form.locator('[name=wigleToken]').fill('synthetic-token');
  const cfgDownload=page.waitForEvent('download');await form.locator('button[type=submit]').click();await(await cfgDownload).saveAs('build/web/upload-synthetic.json');
  const cfg=JSON.parse(await readFile('build/web/upload-synthetic.json','utf8'));assert.equal(cfg.automatic,false);assert.equal(cfg.donate,false);assert.equal(cfg.ssid,'Synthetic-network');
  for(const view of ['install','creator','uploads','guide'])for(const width of [1440,390,320,768]){
    await page.setViewportSize({width,height:1000});await page.locator('.tabs a[href="#'+view+'"]').click();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`${view} overflow at ${width}`);
    if(view==='guide'){await page.locator('#credits').scrollIntoViewIfNeeded();await page.evaluate(()=>window.scrollTo(0,0));}
    await page.screenshot({path:`build/web/${view}-${width}.png`,fullPage:true});
  }
  // Reduced motion: the aura and idle bob hold still, and the studio still works.
  const calm=await browser.newPage({viewport:{width:1280,height:900},reducedMotion:'reduce'});
  calm.on('pageerror',e=>errors.push(e.message));
  await calm.goto(url+'#creator');await calm.locator('#draft-status').filter({hasText:'Draft saved'}).waitFor();
  await calm.locator('#pack-strip button').first().click();
  const frame=()=>calm.locator('#avatar-preview').evaluate(c=>c.toDataURL());
  await calm.waitForTimeout(300);const still=await frame();await calm.waitForTimeout(700);assert.equal(await frame(),still,'preview animates despite reduced motion');
  await calm.close();
  const unsupported=await browser.newPage();await unsupported.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined}));await unsupported.goto(url);await unsupported.locator('#hardware-confirm').check();
  assert(await unsupported.locator('#connect').isDisabled());assert(await unsupported.locator('#upload-avatar').isDisabled());assert.match(await unsupported.locator('#browser-help').textContent(),/Chrome or Edge/);
  assert.deepEqual(errors,[]);
  console.log('Browser checks passed: tabbed tile pickers, style packs, Surprise me, Shuffle, undo/redo, contrast fix, My buddies, scenes, draft persistence, DCAV1/2/3 imports, invalid files, PNG, petting that fades, pinned phone preview, reduced motion, desktop/mobile layouts, unsupported USB, no page errors.');
}finally{await browser.close();}

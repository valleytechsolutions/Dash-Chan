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
  await page.locator('input[name=name]').fill('Captain Mochi');
  await page.getByRole('radio',{name:'Sparkle',exact:true}).check();
  await page.locator('select[name=headwear]').selectOption('5');
  await page.locator('select[name=eyewear]').selectOption('1');
  await page.locator('select[name=neckwear]').selectOption('2');
  await page.locator('#preview-scene').selectOption('2');
  await page.getByRole('button',{name:'Alert',exact:true}).click();
  const downloadEvent=page.waitForEvent('download');await page.locator('#save-avatar').click();
  await (await downloadEvent).saveAs('build/web/avatar.cfg');
  assert.equal(await readFile('build/web/avatar.cfg','utf8'),'DCAV2 0 FFF3D2 FF88B6 1 101029 5 1 2 Captain Mochi\n');
  // A draft survives reload. No USB operation is needed to edit or save.
  await page.reload();await page.locator('#draft-status').filter({hasText:'Draft saved'}).waitFor();
  assert.equal(await page.locator('input[name=name]').inputValue(),'Captain Mochi');
  assert.equal(await page.locator('select[name=headwear]').inputValue(),'5');
  assert(await page.getByRole('radio',{name:'Sparkle',exact:true}).isChecked());
  await page.locator('#import-avatar').setInputFiles({name:'avatar.cfg',mimeType:'text/plain',buffer:Buffer.from('DCAV1 2 112233 FFAABB 2 Old Orbit\n')});
  await page.locator('#avatar-status').filter({hasText:'Buddy imported'}).waitFor();
  assert.equal(await page.locator('input[name=name]').inputValue(),'Old Orbit');
  assert.equal(await page.locator('select[name=neckwear]').inputValue(),'1');
  assert(await page.getByRole('radio',{name:'Orbit · robot',exact:true}).isChecked());
  await page.locator('#import-avatar').setInputFiles({name:'bad.cfg',mimeType:'text/plain',buffer:Buffer.from('<script>not an avatar</script>')});
  await page.locator('#avatar-status.error').waitFor();
  assert.equal(await page.locator('input[name=name]').inputValue(),'Old Orbit');
  await page.getByRole('button',{name:'Use Signal scout preset',exact:true}).click();
  await page.locator('#preview-scene').selectOption('2');
  await page.getByRole('button',{name:'Happy',exact:true}).click();
  await page.locator('#avatar-preview').scrollIntoViewIfNeeded();
  await page.locator('#creator').screenshot({path:'build/web/studio-desktop.png'});
  const art=page.waitForEvent('download');await page.locator('#export-art').click();
  await (await art).saveAs('build/web/portrait.png');
  const png=await readFile('build/web/portrait.png');assert.equal(png.readUInt32BE(16),880);assert.equal(png.readUInt32BE(20),660);assert.equal(png[25],6);
  await page.locator('#avatar-preview').focus();await page.keyboard.press('Enter');
  assert.equal(await page.getByRole('button',{name:'Love',exact:true}).getAttribute('aria-pressed'),'true');
  await page.locator('input[name=name]').fill('Bad;name');assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),false);
  await page.locator('#random-avatar').click();assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),true);
  await page.getByRole('button',{name:'Use Signal scout preset',exact:true}).click();
  await page.getByRole('button',{name:'Happy',exact:true}).click();
  const badImages=await page.locator('img').evaluateAll(imgs=>imgs.filter(i=>!i.complete||!i.naturalWidth).map(i=>i.src));assert.deepEqual(badImages,[]);
  await page.evaluate(()=>window.scrollTo(0,0));await page.screenshot({path:'build/web/desktop.png',fullPage:true});
  for(const width of [390,320,768]){
    await page.setViewportSize({width,height:844});await page.locator('#avatar-preview').scrollIntoViewIfNeeded();
    await page.locator('#creator').screenshot({path:`build/web/studio-${width}.png`});
    const overflow=await page.evaluate(()=>[...document.querySelectorAll('body *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).map(e=>[e.tagName,e.className,Math.round(e.getBoundingClientRect().right)]));
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`Overflow at ${width}px: ${JSON.stringify(overflow)}`);
  }
  const unsupported=await browser.newPage();await unsupported.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined}));await unsupported.goto(url);await unsupported.locator('#hardware-confirm').check();
  assert(await unsupported.locator('#connect').isDisabled());assert(await unsupported.locator('#upload-avatar').isDisabled());assert.match(await unsupported.locator('#browser-help').textContent(),/Chrome or Edge/);
  assert.deepEqual(errors,[]);
  console.log('Browser checks passed: studio outfits, scenes, draft persistence, old/new imports, invalid files, PNG, keyboard petting, desktop/mobile layouts, unsupported USB, no page errors.');
}finally{await browser.close();}

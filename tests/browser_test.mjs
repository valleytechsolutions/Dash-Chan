import {createRequire} from 'node:module';import assert from 'node:assert/strict';import {mkdir,readFile} from 'node:fs/promises';
const {chromium}=createRequire(import.meta.url)('playwright');
const url=process.env.DASH_SITE_URL||'http://127.0.0.1:8765/';
// The CI server starts in the preceding shell line; wait for readiness, not a fixed delay.
for(let attempt=0;;attempt++){try{const response=await fetch(url,{signal:AbortSignal.timeout(2000)});if(!response.ok)throw Error(`HTTP ${response.status}`);break;}catch(e){if(attempt>=19)throw e;await new Promise(resolve=>setTimeout(resolve,250));}}
await mkdir('build/web',{recursive:true});const browser=await chromium.launch({headless:true,...(process.env.DASH_BROWSER?{executablePath:process.env.DASH_BROWSER}:{})});
const errors=[];const page=await browser.newPage({viewport:{width:1440,height:1000}});
page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400)errors.push(`${r.status()} ${r.url()}`);});
await page.goto(url);await page.locator('#flash-status').filter({hasText:'Ready.'}).waitFor();
assert.equal(await page.locator('#connect').isDisabled(),true);await page.locator('#hardware-confirm').check();assert.equal(await page.locator('#connect').isDisabled(),false);
await page.screenshot({path:'build/web/desktop.png',fullPage:true});
await page.locator('#avatar-form input[name=name]').fill('Captain Mochi');await page.locator('select[name=accessory]').selectOption('3');
await page.getByRole('button',{name:'Panic',exact:true}).click();
const downloadEvent=page.waitForEvent('download');await page.locator('#save-avatar').click();const download=await downloadEvent;await download.saveAs('build/web/avatar.cfg');
assert.match(await readFile('build/web/avatar.cfg','utf8'),/^DCAV1 0 FFF3D2 FF88B6 3 Captain Mochi\n$/);
await page.locator('#avatar-form input[name=name]').fill('Bad;name');assert.equal(await page.locator('#avatar-form').evaluate(f=>f.checkValidity()),false);
const badImages=await page.locator('img').evaluateAll(imgs=>imgs.filter(i=>!i.complete||!i.naturalWidth).map(i=>i.src));assert.deepEqual(badImages,[]);
for(const width of [390,320,768]){await page.setViewportSize({width,height:844});await page.evaluate(()=>window.scrollTo(0,0));await page.screenshot({path:`build/web/mobile-${width}.png`,fullPage:true});const overflow=await page.evaluate(()=>[...document.querySelectorAll('body *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).map(e=>[e.tagName,e.className,Math.round(e.getBoundingClientRect().right)]));assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`Horizontal overflow at ${width}px: ${JSON.stringify(overflow)}`);}
const unsupported=await browser.newPage();await unsupported.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined}));await unsupported.goto(url);await unsupported.locator('#hardware-confirm').check();assert(await unsupported.locator('#connect').isDisabled());assert.match(await unsupported.locator('#browser-help').textContent(),/Chrome or Edge/);
assert.deepEqual(errors,[]);await browser.close();console.log('Browser checks passed: desktop, 320/390/768px, images, creator download, unsupported browser, no page errors.');

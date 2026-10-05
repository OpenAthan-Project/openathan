/* Browser tests use the production Digest implementation and simulated device
 * settings. Production settings/API and activation have separate C++ tests. */
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createServer}=require('node:http');
const {readFile}=require('node:fs/promises');
const {join}=require('node:path');
const firmwareDigest=require('./device_digest.cjs');
const {chromium,webkit}=require(require.resolve('playwright',{paths:[join(__dirname,'../web/device-ui')]}));
const rules={standard_offset:0,daylight_offset:0,start:{type:0,time_seconds:0,day:0,month:0,week:0,day_of_week:0},end:{type:0,time_seconds:0,day:0,month:0,week:0,day_of_week:0}};
function initial(){return {time_format:{schema:1,revision:1,hours:24,application:'applied'},schema:1,revision:1,setup:'incomplete',application:'applied',automatic_ready:false,clock_ready:true,scheduler_fault:'none',playing:false,wifi_connected:true,hostname:'openathan-test.local',settings:{latitude:0,longitude:0,timezone:'UTC',timezone_rules:rules,method:'muslim_world_league',asr_method:'standard',high_latitude:'auto',volume:70,offsets:{fajr:0,sunrise:0,dhuhr:0,asr:0,maghrib:0,isha:0},enabled:{fajr:true,dhuhr:true,asr:true,maghrib:true,isha:true}},schedule:{state:'ready',times:[{name:'Fajr',local:'2026-09-25 05:30'},{name:'Dhuhr',local:'2026-09-25 12:30'}],conflicts:[]}};}
async function fixture(){
  const state={device:initial(),lightMutations:0,lightDrop:false,lightFailRead:false,mutations:0,drop:false,failRead:false,posts:[],authenticated:0,
    now:100, challenges:[],cnonces:new Set()};
  const auth=firmwareDigest();
  const server=createServer(async(req,res)=>{
    const header=req.headers.authorization||'';
    let challenge;
    try{challenge=await auth.authorize(state.now,req.method,req.url,header);}
    catch(error){res.writeHead(500);res.end(error.message);return;}
    if(challenge){state.challenges.push(challenge);res.writeHead(401,{'WWW-Authenticate':challenge});res.end();return;}
    state.cnonces.add(/cnonce="([^"]*)"/.exec(header)?.[1]);
    state.authenticated++;
    if(!req.url.startsWith('/api/')){
      const files={'/':['index.html','text/html'],'/app.js':['app.js','text/javascript'],'/style.css':['style.css','text/css']};
      if(!files[req.url]){res.writeHead(404);res.end();return;}
      res.writeHead(200,{'Content-Type':files[req.url][1],'Content-Security-Policy':"default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'"});res.end(await readFile(join(__dirname,'../web/device-ui',files[req.url][0])));return;
    }
    const send=(code,data)=>{res.writeHead(code,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));};
    if(req.method==='GET' && req.url==='/api/firmware'){if(state.failRead){send(503,{error:'Status unavailable'});return;}send(200,state.device.firmware);return;}
    if(req.method==='GET' && req.url==='/api/time-format'){if(state.timeFailRead){send(503,{error:'Read unavailable'});return;}send(200,state.device.time_format);return;}
    if(req.method==='GET' && req.url==='/api/display'){if(state.screenFailRead){send(503,{error:'Read unavailable'});return;}send(200,state.device.display||{supported:false});return;}
    if(req.method==='GET' && req.url==='/api/lights'){if(state.lightFailRead){send(503,{error:'Read unavailable'});return;}send(200,state.device.lights||{supported:false});return;}
    if(req.method==='GET'){
      if(state.failRead && req.url==='/api/status'){send(503,{error:'Status unavailable'});return;}
      if(req.url==='/api/timezones'){
        state.timezoneReads=(state.timezoneReads||0)+1;
        if(state.failTimezones){send(503,{error:'Timezone list unavailable'});return;}
        send(200,{names:['UTC','America/Toronto']});return;
      }
      send(200,state.device);return;
    }
    let body='';for await(const data of req)body+=data;
    const payload=JSON.parse(body);state.posts.push({url:req.url,payload,origin:req.headers.origin});
    if(req.headers.origin!==`http://${req.headers.host}`){send(403,{error:'Same-origin JSON required'});return;}
    if(req.url.startsWith('/api/firmware/')){
      const firmware=state.device.firmware;
      if(req.url.endsWith('/check'))firmware.state='available';
      else if(req.url.endsWith('/install')){firmware.state='queued';firmware.queued_version=payload.version;}
      else if(req.url.endsWith('/cancel')){firmware.state='idle';firmware.queued_version='';}
      if(state.firmwareDrop){state.firmwareDrop=false;res.writeHead(200,{'Content-Type':'application/json'});res.end('{');return;}
      send(200,firmware);return;
    }
    if(req.url==='/api/time-format'){
      if(payload.expected_revision!==state.device.time_format.revision){send(409,{error:'Reload time format before saving'});return;}
      state.timeMutations=(state.timeMutations||0)+1;state.device.time_format.hours=payload.hours;state.device.time_format.revision++;
      if(state.timeDrop){state.timeDrop=false;req.socket.destroy();return;}
      send(200,state.device.time_format);return;
    }
    if(req.url==='/api/display'){
      if(payload.expected_revision!==state.device.display.revision){send(409,{error:'Reload screen brightness before saving'});return;}
      if(state.screenSaveFail){state.device.display.application='save_failed';send(503,{error:'Screen brightness could not be saved; restart the device'});return;}
      if(payload.brightness_percent!==state.device.display.brightness_percent){state.screenMutations=(state.screenMutations||0)+1;state.device.display.brightness_percent=payload.brightness_percent;state.device.display.revision++;}
      if(state.screenDrop){state.screenDrop=false;req.socket.destroy();return;}
      send(200,state.device.display);return;
    }
    if(req.url==='/api/lights'){
      if(payload.expected_revision!==state.device.lights.revision){send(409,{error:'Reload light settings'});return;}
      state.lightMutations++;state.device.lights.settings=payload.settings;state.device.lights.revision++;
      if(state.lightDrop){state.lightDrop=false;req.socket.destroy();return;}
      send(200,state.device.lights);return;
    }
    if(req.url==='/api/preview'){
      const preview=state.device.clock_ready?structuredClone(state.device.schedule):{state:'waiting_for_time'};
      state.previewRequested?.();
      if(state.previewWait)await state.previewWait;
      send(200,preview);return;
    }
    if(['/api/settings','/api/activate'].includes(req.url)){
      if(payload.expected_revision!==state.device.revision){send(409,{error:'Settings changed on another client; reload before saving'});return;}
      state.mutations++;state.device.settings=payload.settings;state.device.revision++;
      if(req.url==='/api/activate'){state.device.setup='active';state.device.automatic_ready=state.device.clock_ready;}
      state.device.next={day:20721,prayer:1,utc:1790339400,name:'Dhuhr',local:'2026-09-25 12:30'};
      if(state.drop){state.drop=false;req.socket.destroy();return;}
      send(200,state.device);return;
    }
    if(req.url==='/api/skip'){
      if(payload.occurrence.utc!==state.device.next.utc){send(409,{error:'Occurrence changed; reload status'});return;}
      state.device.skip=payload.occurrence;
    }else if(req.url==='/api/cancel-skip'){delete state.device.skip;}
    send(200,state.device);
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  return {state,url:`http://127.0.0.1:${server.address().port}`,close:async()=>{
    await new Promise(resolve=>{server.closeAllConnections();server.close(resolve);});await auth.close();
  }};
}
for(const browserName of (process.env.OPENATHAN_TEST_BROWSERS||'chromium').split(',')){
 for(const screenResponse of ['save','reload','reconcile']){
 test(`${browserName}: time format actions retain screen ${screenResponse} state`,async()=>{
  const f=await fixture();f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:10,application:'applied'};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  // Exercise consecutive preference actions before polling can refresh the snapshot.
  await page.addInitScript(()=>{window.setInterval=()=>0;});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#screen-brightness').disabled);
    if(screenResponse==='reload'){
      f.state.device.display.brightness_percent=100;f.state.device.display.revision++;
      await page.locator('#screen-reload').click();
      await page.waitForFunction(()=>document.querySelector('#screen-brightness').value==='100' && !document.querySelector('#screen-brightness').disabled);
    }else{
      f.state.screenDrop=screenResponse==='reconcile';
      await page.locator('#screen-brightness').focus();await page.keyboard.press('End');
      await page.locator('#screen-save').click();
      await page.waitForFunction(()=>/saved\.|confirmed/.test(document.querySelector('#screen-message').textContent));
      assert.equal(f.state.screenMutations,1);
    }
    await page.locator('#time-format').selectOption('12');await page.locator('#time-format-save').click();
    await page.waitForFunction(()=>document.querySelector('#time-format-message').textContent==='Time format saved.');
    assert.equal(await page.locator('#screen-brightness').inputValue(),'100');
    assert.equal(await page.locator('#screen-brightness-value').textContent(),'100%');
    f.state.device.time_format.hours=24;f.state.device.time_format.revision++;
    await page.locator('#time-format-reload').click();
    await page.waitForFunction(()=>document.querySelector('#time-format').value==='24' && !document.querySelector('#time-format').disabled);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'100');
    await page.locator('#screen-brightness').focus();await page.keyboard.press('Home');
    await page.locator('#screen-save').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent==='Screen brightness saved.');
    const lastSave=f.state.posts.filter(post=>post.url==='/api/display').at(-1);
    assert.equal(lastSave.payload.expected_revision,2);
    assert.equal(f.state.device.display.brightness_percent,1);
    assert.equal(f.state.device.display.revision,3);
    assert.equal(f.state.screenMutations,screenResponse==='reload'?1:2);
  }finally{await browser.close();await f.close();}
 });
 }
 test(`${browserName}: screen brightness saves, reconciles and preserves edits`,async()=>{
  const f=await fixture();f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:10,application:'applied'};
  const prayer=structuredClone(f.state.device.settings);
  assert.equal((await fetch(f.url+'/api/display')).status,401);
  assert.equal((await fetch(f.url+'/api/display',{method:'POST',body:'{}'})).status,401);
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'},viewport:{width:390,height:844}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#screen-brightness').disabled);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'10');
    await page.locator('#latitude').fill('44.4');
    await page.locator('#screen-brightness').focus();await page.keyboard.press('Home');
    assert.equal(await page.locator('#screen-brightness-value').textContent(),'1%');
    await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#screen-save').disabled);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'1');
    f.state.screenDrop=true;
    await page.locator('#screen-save').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent.includes('confirmed'));
    assert.equal(f.state.screenMutations,1);
    assert.equal(await page.locator('#latitude').inputValue(),'44.4');
    assert.deepEqual(f.state.device.settings,prayer);
    await page.reload();await page.waitForFunction(()=>document.querySelector('#screen-brightness').value==='1');
    await page.locator('#screen-brightness').focus();await page.keyboard.press('End');
    assert.equal(await page.locator('#screen-brightness-value').textContent(),'100%');
    f.state.device.display.revision++;
    await page.locator('#refresh').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent.includes('another client'));
    assert.equal(await page.locator('#screen-brightness').inputValue(),'100');
    await page.locator('#screen-save').click();await page.waitForFunction(()=>!document.querySelector('#screen-save').disabled);
    assert.equal(f.state.screenMutations,1);
    page.once('dialog',dialog=>dialog.accept());await page.locator('#screen-reload').click();
    await page.waitForFunction(()=>document.querySelector('#screen-brightness').value==='1');
    await page.locator('#screen-brightness').focus();await page.keyboard.press('End');await page.locator('#screen-save').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent==='Screen brightness saved.');
    assert.equal(f.state.device.display.brightness_percent,100);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    if(process.env.OPENATHAN_UI_CAPTURE && browserName==='chromium'){
      await page.locator('#screen-card').screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,'screen-mobile.png')});
      await page.setViewportSize({width:1280,height:900});
      await page.locator('#screen-card').screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,'screen-desktop.png')});
    }
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: screen save failures disable retries until readback`,async()=>{
  const f=await fixture();f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:10,application:'applied'};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#screen-brightness').disabled);
    await page.locator('#screen-brightness').focus();await page.keyboard.press('End');
    f.state.screenDrop=true;f.state.screenFailRead=true;await page.locator('#screen-save').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent.includes('Connection lost'));
    assert.ok(await page.locator('#screen-save').isDisabled());
    assert.equal(f.state.screenMutations,1);
    f.state.screenFailRead=false;page.once('dialog',dialog=>dialog.accept());await page.locator('#screen-reload').click();
    await page.waitForFunction(()=>!document.querySelector('#screen-save').disabled);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'100');
    await page.locator('#screen-brightness').focus();await page.keyboard.press('Home');
    f.state.screenSaveFail=true;await page.locator('#screen-save').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent.includes('could not be saved'));
    assert.ok(await page.locator('#screen-brightness').isDisabled());
    assert.equal(f.state.device.display.brightness_percent,100);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'1');
    assert.ok(await page.locator('#screen-reload').isEnabled());
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: unsupported screen hides controls and backlight faults retain saved value`,async()=>{
  const f=await fixture();const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    assert.ok(await page.locator('#screen-card').isHidden());
    f.state.device.display={schema:1,supported:true,revision:2,brightness_percent:37,application:'output_unavailable'};
    await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#screen-card').hidden);
    assert.equal(await page.locator('#screen-brightness').inputValue(),'37');
    assert.match(await page.locator('#screen-message').textContent(),/backlight is unavailable/);
    f.state.device.display.application='storage_fault';await page.locator('#refresh').click();
    await page.waitForFunction(()=>document.querySelector('#screen-message').textContent.includes('could not be read'));
    assert.ok(await page.locator('#screen-save').isDisabled());
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: time format persists, reconciles a dropped save and preserves conflicting edits`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  f.state.device.schedule.times=[{name:'Fajr',local:'2026-09-25 00:00'},{name:'Dhuhr',local:'2026-09-25 12:00'},{name:'Asr',local:'2026-09-25 13:30'}];
  f.state.device.next={name:'Asr',local:'2026-09-25 13:30'};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'},viewport:{width:390,height:844}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#time-format').disabled);
    assert.equal(await page.locator('#times dd').allTextContents().then(v=>v.join(',')),'00:00,12:00,13:30');
    const revision=f.state.device.revision,settings=structuredClone(f.state.device.settings);
    await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#preview-section').hidden);
    await page.locator('#time-format').selectOption('12');f.state.timeDrop=true;
    await page.locator('#time-format-save').click();
    await page.waitForFunction(()=>document.querySelector('#time-format-message').textContent.includes('confirmed'));
    assert.deepEqual(await page.locator('#times dd').allTextContents(),['12:00 AM','12:00 PM','1:30 PM']);
    assert.deepEqual(await page.locator('#preview-times dd').allTextContents(),['12:00 AM','12:00 PM','1:30 PM']);
    assert.ok((await page.locator('#next').textContent()).includes('2026-09-25 1:30 PM'));
    assert.equal(f.state.timeMutations,1);assert.equal(f.state.device.revision,revision);assert.deepEqual(f.state.device.settings,settings);
    await page.reload();await page.waitForFunction(()=>!document.querySelector('#time-format').disabled);
    assert.equal(await page.locator('#time-format').inputValue(),'12');
    await page.locator('#time-format').selectOption('24');f.state.device.time_format.revision++;
    await page.locator('#refresh').click();
    await page.waitForFunction(()=>document.querySelector('#time-format-message').textContent.includes('another client'));
    assert.equal(await page.locator('#time-format').inputValue(),'24');
    await page.locator('#time-format-save').click();
    await page.waitForFunction(()=>!document.querySelector('#time-format-save').disabled);
    assert.equal(f.state.timeMutations,1);
    page.once('dialog',dialog=>dialog.accept());await page.locator('#time-format-reload').click();
    await page.waitForFunction(()=>document.querySelector('#time-format').value==='12');
    await page.locator('#time-format').selectOption('24');await page.locator('#time-format-save').click();
    await page.waitForFunction(()=>document.querySelector('#time-format-message').textContent==='Time format saved.');
    assert.deepEqual(await page.locator('#times dd').allTextContents(),['00:00','12:00','13:30']);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  } finally {await browser.close();await f.close();}
 });
 test(`${browserName}: a superseded queue clears its reconnect hint and permits checks`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.4.0',state:'current',result:'superseded',error:''};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  await page.addInitScript(()=>sessionStorage.setItem('firmware-expected','v0.3.0'));
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#firmware-section').hidden);
    assert.equal(await page.evaluate(()=>sessionStorage.getItem('firmware-expected')),null);
    assert.ok(await page.locator('#firmware-check').isEnabled());
    await page.locator('#firmware-check').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('available'));
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/check').length,1);
  }finally{await browser.close();await f.close();}
 });
 for(const denied of ['access','getItem','setItem','removeItem']) {
  test(`${browserName}: denied storage ${denied} preserves settings and firmware actions`,async()=>{
   const f=await fixture();f.state.device.setup='active';f.state.device.automatic_ready=true;
   f.state.device.settings.latitude=44.4;
   f.state.device.firmware={version:'v0.2.0',state:'available',available:{version:'v0.3.0'},result:'',error:''};
   const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
   const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
   const errors=[];page.on('pageerror',error=>errors.push(error.message));
   await page.addInitScript(operation=>{
    const denied=()=>{throw new DOMException('Browser storage denied','SecurityError');};
    if(operation==='access')Object.defineProperty(window,'sessionStorage',{get:denied});
    else Object.defineProperty(Storage.prototype,operation,{value:denied});
   },denied);
   try {
    await page.goto(f.url);await page.waitForFunction(()=>document.querySelector('#latitude').value==='44.4' && !document.querySelector('#firmware-install').disabled);
    await page.locator('#latitude').fill('45.2');
    await page.locator('#firmware-install').click();
    await page.waitForFunction(()=>!document.querySelector('#firmware-cancel').hidden && !document.querySelector('#firmware-cancel').disabled);
    assert.equal(await page.locator('#latitude').inputValue(),'45.2');
    await page.locator('#firmware-cancel').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-cancel').hidden && !document.querySelector('#firmware-install').disabled);
    await page.locator('#firmware-install').click();
    await page.waitForFunction(()=>!document.querySelector('#firmware-cancel').hidden && !document.querySelector('#firmware-cancel').disabled);
    f.state.device.firmware={version:'v0.3.0',state:'success',result:'success',error:''};
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('updated successfully') && !document.querySelector('#firmware-check').disabled);
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/install').length,2);
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/cancel').length,1);
    await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#message').textContent==='Device updated.');
    assert.equal(f.state.device.settings.latitude,45.2);assert.deepEqual(errors,[]);
   }finally{await browser.close();await f.close();}
  });
 }
 test(`${browserName}: a queued task-start failure stays visibly cancellable`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.2.0',state:'queued',queued_version:'v0.3.0',error:'Not enough memory to start the update; retrying after five minutes'};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#firmware-cancel').hidden);
    assert.match(await page.locator('#firmware-status').textContent(),/retrying after five minutes/);
    assert.ok(await page.locator('#firmware-check').isDisabled());
    assert.ok(await page.locator('#firmware-cancel').isEnabled());
    await page.locator('#firmware-cancel').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-cancel').hidden);
    assert.equal(f.state.device.firmware.queued_version,'');
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/cancel').length,1);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: uncertain install blocks another action until status returns`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.2.0',state:'available',available:{version:'v0.3.0'},result:'',error:''};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#firmware-install').hidden);
    f.state.firmwareDrop=true;f.state.failRead=true;
    await page.locator('#firmware-install').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('Waiting to read update status'));
    assert.ok(await page.locator('#firmware-install').isDisabled());
    assert.ok(await page.locator('#firmware-check').isDisabled());
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/install').length,1);
    f.state.failRead=false;
    await page.waitForFunction(()=>!document.querySelector('#firmware-cancel').hidden && !document.querySelector('#firmware-cancel').disabled);
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/install').length,1);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: firmware queue and uncertain response preserve edits`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.2.0',state:'available',received:0,last_check:1700000000,available:{version:'v0.3.0',bytes:1000},result:'',error:''};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({viewport:{width:390,height:844},httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#firmware-install').hidden);
    await page.locator('#latitude').fill('44.4');
    f.state.firmwareDrop=true;await page.locator('#firmware-install').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('queued'));
    assert.equal(f.state.posts.filter(p=>p.url==='/api/firmware/install').length,1);
    assert.deepEqual(f.state.posts.find(p=>p.url==='/api/firmware/install').payload,{version:'v0.3.0'});
    assert.equal(await page.locator('#latitude').inputValue(),'44.4');
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    assert.ok(await page.locator('#firmware-check').isDisabled());
    await page.screenshot({path:join(process.env.OPENATHAN_TEST_OUTPUT_DIR || require('node:os').tmpdir(),`openathan-upgrade-mobile-${browserName}.png`),fullPage:true});
    await page.locator('#firmware-cancel').click();
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('No update'));
    assert.equal(f.state.device.firmware.queued_version,'');assert.equal(f.state.mutations,0);
    f.state.device.firmware.supported=false;
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('maintainer USB'));
    assert.ok(await page.locator('#firmware-install').isDisabled());
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: firmware reconnect reports rollback and success`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.2.0',state:'queued',queued_version:'v0.3.0',result:'',error:''};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('queued'));
    f.state.device.firmware.state='rolled_back';f.state.device.firmware.result='rolled_back';
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('previous firmware'));
    f.state.device.firmware.version='v0.3.0';f.state.device.firmware.state='success';f.state.device.firmware.result='success';
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('updated successfully'));
    await page.screenshot({path:join(process.env.OPENATHAN_TEST_OUTPUT_DIR || require('node:os').tmpdir(),`openathan-upgrade-desktop-${browserName}.png`),fullPage:true});
    assert.equal(f.state.posts.filter(p=>p.url.startsWith('/api/firmware/')).length,0);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: pending handoff survives a transient timezone-list failure`,async()=>{
  const f=await fixture();f.state.failTimezones=true;f.state.device.setup='active';
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url+'/#v=1&latitude=44.4&longitude=-79.7&timezone=America%2FToronto&source=browser');
    await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('location suggestion is waiting'));
    assert.equal(new URL(page.url()).hash,'');
    assert.equal(await page.locator('#location-review').isVisible(),false);
    assert.equal(await page.locator('#latitude').inputValue(),'0');
    assert.equal(await page.locator('#timezone').inputValue(),'UTC');
    assert.equal(f.state.mutations,0);
    f.state.failTimezones=false;
    await page.locator('#refresh').click();
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    assert.equal(await page.locator('#latitude').inputValue(),'44.4');
    assert.equal(await page.locator('#longitude').inputValue(),'-79.7');
    assert.equal(await page.locator('#timezone').inputValue(),'America/Toronto');
    assert.ok(await page.locator('#save').isDisabled());
    assert.ok(f.state.timezoneReads>=2);
    assert.equal(f.state.mutations,0);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: pending handoff survives an initial status failure`,async()=>{
  const f=await fixture();f.state.failRead=true;
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url+'/#v=1&latitude=44.4&longitude=-79.7&timezone=America%2FToronto&source=browser');
    await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('Status unavailable'));
    assert.equal(new URL(page.url()).hash,'');
    assert.equal(await page.locator('#location-review').isVisible(),false);
    assert.ok(await page.locator('#latitude').isDisabled());
    f.state.failRead=false;
    await page.locator('#refresh').click();
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    assert.equal(await page.locator('#latitude').inputValue(),'44.4');
    assert.equal(await page.locator('#longitude').inputValue(),'-79.7');
    assert.equal(await page.locator('#timezone').inputValue(),'America/Toronto');
    assert.ok(await page.locator('#save').isDisabled());
    assert.equal(f.state.mutations,0);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: suggested location requires review and never saves automatically`,async()=>{
  const f=await fixture();
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    const handoff='#v=1&latitude=44.4113861&longitude=-79.6819456&timezone=America%2FToronto&source=ip&accuracy=1000';
    await page.goto(f.url+'/'+handoff);
    await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    assert.equal(new URL(page.url()).hash,'');
    assert.equal(await page.locator('#latitude').inputValue(),'44.4113861');
    assert.equal(await page.locator('#longitude').inputValue(),'-79.6819456');
    assert.equal(await page.locator('#timezone').inputValue(),'America/Toronto');
    assert.match(await page.locator('#location-review').textContent(),/Approximate IP location.*1000 km/);
    assert.ok(await page.locator('#save').isDisabled());assert.equal(f.state.mutations,0);
    await page.locator('#method').selectOption('north_america');
    await page.locator('#preview').click();
    await page.waitForFunction(()=>!document.querySelector('#save').disabled);
    assert.equal(f.state.mutations,0);
    await page.locator('#longitude').fill('-80');assert.ok(await page.locator('#save').isDisabled());
    await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#save').disabled);
    await page.locator('#save').click();
    await page.waitForFunction(()=>document.querySelector('#setup-state').textContent==='Setup complete');
    assert.equal(f.state.mutations,1);
    assert.equal(f.state.device.settings.longitude,-80);
    assert.equal(f.state.device.settings.timezone,'America/Toronto');
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: unsupported timezone and invalid handoff keep manual settings safe`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url+'/#v=1&latitude=0&longitude=0&timezone=Not%2FSupported&source=browser&accuracy=12');
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    assert.equal(await page.locator('#latitude').inputValue(),'0');
    assert.equal(await page.locator('#longitude').inputValue(),'0');
    assert.equal(await page.locator('#timezone').inputValue(),'');
    assert.match(await page.locator('#location-review').textContent(),/Choose a timezone supported by this device/);
    assert.equal(f.state.mutations,0);
    await page.goto(f.url+'/#v=1&latitude=0&longitude=0&source=browser');
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    assert.equal(await page.locator('#timezone').inputValue(),'');
    await page.goto(f.url+'/#v=1&latitude=999&longitude=0&source=ip');
    await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('Location link was invalid'));
    assert.equal(await page.locator('#latitude').inputValue(),'0');
    assert.equal(f.state.mutations,0);
    await page.locator('#latitude').fill('12');
    const handoff='#v=1&latitude=44.4&longitude=-79.7&source=browser';
    page.once('dialog',dialog=>dialog.dismiss());
    await page.evaluate(hash=>{location.hash=hash;},handoff);
    await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('unsaved edits were kept'));
    assert.equal(await page.locator('#latitude').inputValue(),'12');
    page.once('dialog',dialog=>dialog.accept());
    await page.evaluate(hash=>{location.hash=hash;},handoff);
    await page.waitForFunction(()=>document.querySelector('#latitude').value==='44.4');
    assert.ok(await page.locator('#save').isDisabled());
    assert.equal(f.state.mutations,0);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: a late preview cannot approve newer coordinates`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url+'/#v=1&latitude=44.4&longitude=-79.7&timezone=UTC&source=browser');
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    let releasePreview;f.state.previewWait=new Promise(resolve=>{releasePreview=resolve;});
    const requested=new Promise(resolve=>{f.state.previewRequested=resolve;});
    await page.locator('#preview').click();await requested;
    page.once('dialog',dialog=>dialog.accept());
    await page.evaluate(()=>{location.hash='#v=1&latitude=43.7&longitude=-79.4&timezone=UTC&source=ip';});
    await page.waitForFunction(()=>document.querySelector('#latitude').value==='43.7');
    releasePreview();f.state.previewWait=null;f.state.previewRequested=null;
    await page.waitForFunction(()=>!document.querySelector('#preview').disabled);
    assert.equal(await page.locator('#preview-section').isVisible(),false);
    assert.ok(await page.locator('#save').isDisabled());
    assert.equal(f.state.mutations,0);
    await page.locator('#preview').click();
    await page.waitForFunction(()=>!document.querySelector('#save').disabled);
    const previews=f.state.posts.filter(post=>post.url==='/api/preview');
    assert.equal(previews.length,2);
    assert.equal(previews[0].payload.settings.latitude,44.4);
    assert.equal(previews[1].payload.settings.latitude,43.7);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: clock must be ready to review a suggested timetable`,async()=>{
  const f=await fixture();f.state.device.clock_ready=false;
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url+'/#v=1&latitude=44.4&longitude=-79.7&timezone=UTC&source=browser');
    await page.waitForFunction(()=>!document.querySelector('#location-review').hidden);
    await page.locator('#method').selectOption('north_america');
    await page.locator('#preview').click();
    await page.waitForFunction(()=>document.querySelector('#preview-message').textContent.includes('after the clock is ready'));
    assert.ok(await page.locator('#save').isDisabled());
    assert.equal(f.state.mutations,0);
    f.state.device.clock_ready=true;
    await page.locator('#preview').click();
    await page.waitForFunction(()=>!document.querySelector('#save').disabled);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: light preferences, conflicts and uncertain saves`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  f.state.device.lights={supported:true,schema:1,revision:1,application:'applied',mode:'green',settings:{enabled:true,brightness_percent:20}};
  const original=structuredClone(f.state.device.settings);
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async error=>{await f.close();throw error;});
  const page=await browser.newPage({viewport:{width:390,height:844},httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#lights-fields').disabled);
    await page.locator('#lights-brightness').fill('35');
    await page.locator('#refresh').click();
    await page.waitForFunction(()=>!document.querySelector('#lights-save').disabled);
    assert.equal(await page.locator('#lights-brightness').inputValue(),'35');
    f.state.lightDrop=true;await page.locator('#lights-save').click();
    await page.waitForFunction(()=>document.querySelector('#lights-message').textContent.includes('confirmed after reconnecting'));
    assert.equal(f.state.lightMutations,1);assert.deepEqual(f.state.device.settings,original);
    await page.locator('#lights-brightness').fill('45');f.state.device.lights.revision++;
    await page.locator('#lights-save').click();
    await page.waitForFunction(()=>document.querySelector('#lights-message').textContent.includes('another client'));
    assert.equal(await page.locator('#lights-brightness').inputValue(),'45');assert.equal(f.state.lightMutations,1);
    page.on('dialog',dialog=>dialog.accept());await page.locator('#lights-reload').click();
    await page.waitForFunction(()=>document.querySelector('#lights-brightness').value==='35');
    await page.locator('#lights-enabled').uncheck();
    assert.ok(await page.locator('#lights-brightness').isDisabled());
    f.state.lightDrop=true;f.state.lightFailRead=true;await page.locator('#lights-save').click();
    await page.waitForFunction(()=>document.querySelector('#lights-message').textContent.includes('Connection lost'));
    assert.ok(await page.locator('#lights-save').isDisabled());assert.equal(f.state.lightMutations,2);
    f.state.lightFailRead=false;await page.locator('#lights-reload').click();
    await page.waitForFunction(()=>!document.querySelector('#lights-save').disabled);
    assert.equal(await page.locator('#lights-enabled').isChecked(),false);
    if(process.env.OPENATHAN_UI_SCREENSHOTS)await page.screenshot({path:join(process.env.OPENATHAN_UI_SCREENSHOTS,'ui-mobile-'+browserName+'.png'),fullPage:true});
    await page.setViewportSize({width:1280,height:960});
    if(process.env.OPENATHAN_UI_SCREENSHOTS)await page.screenshot({path:join(process.env.OPENATHAN_UI_SCREENSHOTS,'ui-desktop-'+browserName+'.png'),fullPage:true});
    f.state.device.lights.application='output_unavailable';await page.locator('#refresh').click();
    await page.waitForFunction(()=>document.querySelector('#lights-message').textContent.includes('unavailable'));
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    f.state.device.lights.supported=false;await page.locator('#refresh').click();
    await page.waitForFunction(()=>document.querySelector('#lights-card').hidden);
  } finally {await browser.close();await f.close();}
 });
 test(`${browserName}: sustained authentication and transparent nonce renewal`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async(error)=>{await f.close();throw error;});
  const context=await browser.newContext(browserName==='chromium'?{}:{httpCredentials:{username:'admin',password:'browser test password'}});
  const page=await context.newPage();let prompts=0;
  try{
    if(browserName==='chromium'){
      // Model one user sign-in. Reject any later credential prompt so the
      // test cannot hide a broken renewal by silently supplying the password.
      const cdp=await context.newCDPSession(page);
      await cdp.send('Fetch.enable',{handleAuthRequests:true});
      cdp.on('Fetch.requestPaused',e=>cdp.send('Fetch.continueRequest',{requestId:e.requestId}));
      cdp.on('Fetch.authRequired',e=>{
        prompts++;
        cdp.send('Fetch.continueWithAuth',{requestId:e.requestId,authChallengeResponse:prompts===1
          ?{response:'ProvideCredentials',username:'admin',password:'browser test password'}:{response:'CancelAuth'}});
      });
    }
    await page.goto(f.url);await page.waitForFunction(()=>document.querySelector('#message').textContent==='Connected to your OpenAthan.');
    const initialChallenges=f.state.challenges.length;
    const responses=await page.evaluate(async()=>{
      const result=[];
      for(let i=0;i<25;i++)result.push(...await Promise.all(Array.from({length:4},()=>fetch('/api/status').then(r=>r.status))));
      return result;
    });
    assert.ok(responses.every(code=>code===200));
    assert.equal(f.state.challenges.length,initialChallenges);
    if(browserName==='chromium'){assert.ok(f.state.cnonces.size>8);assert.equal(prompts,1);}
    // Expire the real verifier's nonce; a challenged POST must renew before it
    // reaches the mutation handler and commit only once.
    f.state.now+=300000;
    const saved=await page.evaluate(async body=>{
      return fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify(body)}).then(r=>r.status);
    },{schema:1,expected_revision:f.state.device.revision,settings:{...f.state.device.settings,volume:35}});
    assert.equal(saved,200);assert.equal(f.state.mutations,1);
    const renewed=f.state.challenges.slice(initialChallenges);
    assert.equal(renewed.length,1);assert.match(renewed[0],/stale=true/);
    if(browserName==='chromium')assert.equal(prompts,1);
    assert.equal(f.state.device.settings.volume,35);
    f.state.now+=300000;
    assert.equal(await page.evaluate(()=>fetch('/api/status').then(r=>r.status)),200);
    assert.equal(f.state.challenges.length,initialChallenges+2);
    assert.match(f.state.challenges.at(-1),/stale=true/);
    if(browserName==='chromium')assert.equal(prompts,1);
  }finally{await context.close();await browser.close();await f.close();}
 });
 test(`${browserName}: coordinate precision survives volume saves and lost responses`,async()=>{
  const f=await fixture();f.state.device.setup='active';
  const latitude=43.653212345678909,longitude=-79.383212345678913;
  Object.assign(f.state.device.settings,{latitude,longitude});
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async(error)=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try{
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    assert.equal(Number(await page.locator('#latitude').inputValue()),latitude);
    assert.equal(Number(await page.locator('#longitude').inputValue()),longitude);
    await page.locator('#volume').fill('35');f.state.drop=true;
    await page.locator('#save').click();
    await page.waitForFunction(()=>/response was lost|another client/.test(document.querySelector('#message').textContent));
    assert.equal(f.state.mutations,1);
    const writes=f.state.posts.filter(p=>p.url==='/api/settings');assert.ok(writes.length>=1);
    // Chromium may replay a failed transport; every replay must retain the
    // original revision so it cannot produce a second durable mutation.
    assert.ok(writes.every(p=>p.payload.expected_revision===1));
    assert.equal(writes[0].payload.settings.latitude,latitude);
    assert.equal(writes[0].payload.settings.longitude,longitude);
    assert.equal(writes[0].payload.settings.volume,35);
    assert.equal(Number(await page.locator('#latitude').inputValue()),latitude);
    assert.equal(Number(await page.locator('#longitude').inputValue()),longitude);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: setup, concurrency, uncertain saves, and mobile layout`,async()=>{
  const f=await fixture();const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async(error)=>{await f.close();throw error;});
  const context=await browser.newContext({httpCredentials:{username:'admin',password:'browser test password'},viewport:{width:1280,height:900}});
  const page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  try{
    await page.goto(f.url);await page.locator('#fields').waitFor();
    await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    assert.equal(await page.locator('#test-banner').isVisible(),false);
    assert.equal(await page.locator('#latitude').inputValue(),'');
    assert.equal(await page.locator('#method').inputValue(),'');
    await page.locator('#latitude').fill('44.3894');await page.locator('#longitude').fill('-79.6903');
    await page.locator('#timezone').fill('UTC');await page.locator('#method').selectOption('north_america');
    await page.locator('#preview').click();await page.locator('#preview-section').waitFor({state:'visible'});assert.equal(f.state.mutations,0);
    await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#save').textContent==='Save settings');
    assert.equal(f.state.mutations,1);assert.equal(f.state.device.settings.latitude,44.3894);
    await page.locator('#latitude').fill('45');f.state.device.revision++;f.state.device.settings.latitude=46;
    await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('another client'));
    assert.equal(await page.locator('#latitude').inputValue(),'45');assert.equal(f.state.mutations,1);
    page.on('dialog',dialog=>dialog.accept());await page.locator('#reload').click();await page.waitForFunction(()=>document.querySelector('#latitude').value==='46');
    await page.locator('#longitude').fill('-80');f.state.drop=true;await page.locator('#save').click();
    await page.waitForFunction(()=>/response was lost|another client/.test(document.querySelector('#message').textContent));
    assert.equal(f.state.mutations,2);assert.equal(f.state.device.settings.longitude,-80);
    await page.locator('#reload').click();await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    // The displayed occurrence is captured before a server-side change.
    f.state.device.next.utc++;await page.locator('#skip').click();
    await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('Occurrence changed'));
    assert.equal(f.state.device.skip,undefined);
    f.state.failRead=true;await page.locator('#reload').click();
    await page.waitForFunction(()=>!document.querySelector('#reload').disabled);
    assert.match(await page.locator('#message').textContent(),/Status unavailable/);
    f.state.failRead=false;f.state.device.scheduler_fault='audio unavailable';
    await page.locator('#reload').click();await page.waitForFunction(()=>document.querySelector('#health').textContent.includes('compatible audio installation'));
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>window.innerWidth),false);
    await page.screenshot({path:join(process.env.OPENATHAN_TEST_OUTPUT_DIR || require('node:os').tmpdir(),`openathan-device-ui-${browserName}-mobile.png`),fullPage:true});
    assert.deepEqual(errors,[]);assert.ok(f.state.authenticated>5);assert.ok(f.state.posts.every(p=>p.origin===f.url));
  }finally{await context.close();await browser.close();await f.close();}
 });
 test(`${browserName}: activation waits for a synchronized clock`,async()=>{
  const f=await fixture();f.state.device.clock_ready=false;f.state.device.test_mode=true;
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}).catch(async(error)=>{await f.close();throw error;});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'},viewport:{width:390,height:844}});
  try{
    await page.goto(f.url);await page.waitForFunction(()=>!document.querySelector('#fields').disabled);
    assert.equal(await page.locator('#test-banner').isVisible(),true);
    assert.match(await page.locator('#test-banner').textContent(),/Test firmware/);
    await page.locator('#latitude').fill('0');await page.locator('#longitude').fill('0');await page.locator('#timezone').fill('UTC');await page.locator('#method').selectOption('muslim_world_league');
    await page.locator('#preview').click();await page.waitForFunction(()=>document.querySelector('#preview-message').textContent.includes('Waiting for time'));
    await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#setup-state').textContent==='Setup complete');
    assert.equal(f.state.device.automatic_ready,false);
    assert.match(await page.locator('#health').textContent(),/Waiting for time synchronization/);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>window.innerWidth),false);
    await page.screenshot({path:join(process.env.OPENATHAN_TEST_OUTPUT_DIR || require('node:os').tmpdir(),`openathan-test-firmware-${browserName}.png`),fullPage:true});
  }finally{await browser.close();await f.close();}
 });
}

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
function initial(){
 const times=['05:30','06:45','12:30','15:45','18:20','19:40'].map((time,i)=>({name:['Fajr','Sunrise','Dhuhr','Asr','Maghrib','Isha'][i],local:'2026-09-25 '+time,utc:Date.parse('2026-09-25T'+time+':00Z')/1000}));
 return {time_format:{schema:1,revision:1,hours:24,application:'applied'},schema:1,revision:1,setup:'active',application:'applied',automatic_ready:true,clock_ready:true,local_date:'2026-09-25',scheduler_fault:'none',playing:false,wifi_connected:true,hostname:'openathan-test.local',settings:{latitude:0,longitude:0,timezone:'UTC',timezone_rules:rules,method:'muslim_world_league',asr_method:'standard',high_latitude:'auto',volume:70,offsets:{fajr:0,sunrise:0,dhuhr:0,asr:0,maghrib:0,isha:0},enabled:{fajr:true,dhuhr:true,asr:true,maghrib:true,isha:true}},schedule:{state:'ready',times,conflicts:[]},next:{day:20721,prayer:2,name:'Asr',local:times[3].local,utc:times[3].utc}};
}
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
      if(state.timeDrop){state.timeDrop=false;res.writeHead(200,{'Content-Type':'application/json'});res.end('{');return;}
      send(200,state.device.time_format);return;
    }
    if(req.url==='/api/display'){
      if(payload.expected_revision!==state.device.display.revision){send(409,{error:'Reload screen brightness before saving'});return;}
      if(state.screenSaveFail){state.device.display.application='save_failed';send(503,{error:'Screen brightness could not be saved; restart the device'});return;}
      if(payload.brightness_percent!==state.device.display.brightness_percent){state.screenMutations=(state.screenMutations||0)+1;state.device.display.brightness_percent=payload.brightness_percent;state.device.display.revision++;}
      if(state.screenDrop){state.screenDrop=false;res.writeHead(200,{'Content-Type':'application/json'});res.end('{');return;}
      send(200,state.device.display);return;
    }
    if(req.url==='/api/lights'){
      if(payload.expected_revision!==state.device.lights.revision){send(409,{error:'Reload light settings'});return;}
      state.lightMutations++;state.device.lights.settings=payload.settings;state.device.lights.revision++;
      if(state.lightDrop){state.lightDrop=false;res.writeHead(200,{'Content-Type':'application/json'});res.end('{');return;}
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
      if(state.writeWait){state.writeRequested?.();await state.writeWait;}
      if(state.drop){state.drop=false;res.writeHead(200,{'Content-Type':'application/json'});res.end('{');return;}
      send(200,state.device);return;
    }
    if(req.url==='/api/skip'){
      if(payload.expected_revision!==state.device.revision||payload.occurrence.utc!==state.device.next.utc){send(409,{error:'Occurrence changed; reload status'});return;}
      state.device.skip={day:payload.occurrence.day,prayer:payload.occurrence.prayer};
    }else if(req.url==='/api/cancel-skip'){delete state.device.skip;}else if(req.url==='/api/stop'&&!state.stopStillPlaying){state.device.playing=false;}
    if(state.actionDrop&&['/api/skip','/api/cancel-skip','/api/stop'].includes(req.url)){state.actionDrop=false;res.writeHead(200);res.end('{');return;}
    send(200,state.device);
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  return {state,url:`http://127.0.0.1:${server.address().port}`,close:async()=>{
    await new Promise(resolve=>{server.closeAllConnections();server.close(resolve);});await auth.close();
  }};
}
async function open(f,name,options={}){
 const browser=await ({chromium,webkit}[name]).launch({headless:true});
 const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'},...options});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto(f.url);
 await page.waitForFunction(()=>document.querySelector('#latitude').value!==''||document.querySelector('#setup-progress').hidden===false||document.querySelector('#readiness-text').textContent==='Saved storage unavailable');
 return {browser,page,errors,close:async()=>{assert.deepEqual(errors,[]);await browser.close();await f.close();}};
}
async function settings(page){await page.locator('[data-view="settings"]').click();}
async function slide(page,id,value){await page.locator('#'+id).evaluate((el,v)=>{el.value=v;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},String(value));}
async function saved(page,id){await page.waitForFunction(id=>/Saved/.test(document.querySelector('#'+id+'-feedback').textContent),id);}
async function refresh(page){await settings(page);await page.locator('#refresh').click();await page.locator('[data-view="today"]').click();}
for(const browserName of (process.env.OPENATHAN_TEST_BROWSERS||'chromium').split(',')){
 test(`${browserName}: a superseded queue clears its reconnect hint and permits checks`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.4.0',state:'current',result:'superseded',error:''};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  await page.addInitScript(()=>sessionStorage.setItem('firmware-expected','v0.3.0'));
  try {
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>!document.querySelector('#firmware-section').hidden);
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
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>document.querySelector('#latitude').value==='44.4' && !document.querySelector('#firmware-install').disabled);
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
    await page.locator('#preview').click();await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent==='Prayer settings saved');
    assert.equal(f.state.device.settings.latitude,45.2);assert.deepEqual(errors,[]);
   }finally{await browser.close();await f.close();}
  });
 }
 test(`${browserName}: a queued task-start failure stays visibly cancellable`,async()=>{
  const f=await fixture();f.state.device.firmware={version:'v0.2.0',state:'queued',queued_version:'v0.3.0',error:'Not enough memory to start the update; retrying after five minutes'};
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true});
  const page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try {
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>!document.querySelector('#firmware-cancel').hidden);
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
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>!document.querySelector('#firmware-install').hidden);
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
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>!document.querySelector('#firmware-install').hidden);
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
    await page.goto(f.url);await page.locator('[data-view="settings"]').click();await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('queued'));
    f.state.device.firmware.state='rolled_back';f.state.device.firmware.result='rolled_back';
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('previous firmware'));
    f.state.device.firmware.version='v0.3.0';f.state.device.firmware.state='success';f.state.device.firmware.result='success';
    await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('updated successfully'));
    await page.screenshot({path:join(process.env.OPENATHAN_TEST_OUTPUT_DIR || require('node:os').tmpdir(),`openathan-upgrade-desktop-${browserName}.png`),fullPage:true});
    assert.equal(f.state.posts.filter(p=>p.url.startsWith('/api/firmware/')).length,0);
  }finally{await browser.close();await f.close();}
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
    await page.goto(f.url);await page.waitForFunction(()=>document.querySelector('#volume-value').textContent==='70%');
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
 test(`${browserName}: upcoming occurrence survives skip/restore and follows authoritative advancement`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   assert.equal(await page.locator('#next-name').textContent(),'Asr');assert.equal(await page.locator('[aria-current="time"]').getAttribute('data-prayer'),'asr');
   await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#readiness-text').textContent==='Athan skipped today');
   assert.equal(await page.locator('#next-name').textContent(),'Asr');assert.equal(await page.locator('[aria-current="time"]').count(),1);assert.match(await page.locator('[aria-current="time"] small').textContent(),/Skipped/);assert.match(await page.locator('#skip').textContent(),/Restore Asr today/);
   await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#readiness-text').textContent==='Ready to play');
   assert.equal(await page.locator('[aria-current="time"]').getAttribute('data-prayer'),'asr');
   f.state.device.next={...f.state.device.next,prayer:3,name:'Maghrib',...f.state.device.schedule.times[4]};await refresh(page);await page.waitForFunction(()=>document.querySelector('#next-name').textContent==='Maghrib');
   assert.equal(await page.locator('[aria-current="time"]').getAttribute('data-prayer'),'maghrib');
   f.state.device.next={day:20722,prayer:0,name:'Fajr',utc:Date.parse('2026-09-26T05:30Z')/1000,local:'2026-09-26 05:30'};await refresh(page);await page.waitForFunction(()=>document.querySelector('#next-name').textContent==='Fajr');
   assert.equal(await page.locator('[aria-current="time"]').count(),0);assert.ok(await page.locator('#next-date').isVisible());
  }finally{await o.close();}
 });
 test(`${browserName}: disconnected status freezes the board and waiting time clears it`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   f.state.device.playing=true;await refresh(page);await page.waitForFunction(()=>!document.querySelector('#stop').hidden);
   f.state.failRead=true;await refresh(page);await page.waitForFunction(()=>!document.querySelector('#connection-banner').hidden);
   assert.equal(await page.locator('#next-name').textContent(),'Asr');assert.equal(await page.locator('[aria-current="time"]').count(),1);assert.match(await page.locator('#connection-detail').textContent(),/stale.*unknown/);assert.ok(await page.locator('#stop').isDisabled());
   f.state.failRead=false;f.state.device.clock_ready=false;f.state.device.next=undefined;f.state.device.schedule={state:'waiting_for_time'};await page.locator('#reconnect').click();await page.waitForFunction(()=>document.querySelector('#next-name').textContent==='—');assert.equal(await page.locator('[aria-current="time"]').count(),0);
  }finally{await o.close();}
 });
 test(`${browserName}: preference saves preserve coordinate precision and calculation drafts`,async()=>{
  const f=await fixture();f.state.device.settings.latitude=43.6532123456789;f.state.device.settings.longitude=-79.3832123456789;
  const o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#method').selectOption('north_america');await page.locator('#asr').selectOption('hanafi');await page.locator('#calculation-fields summary').click();await page.locator('#high-latitude').selectOption('middle_of_night');await page.locator('#offset-asr').fill('3');await page.locator('#enabled-asr').uncheck();
   await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent==='Saved to speaker');
   assert.equal(f.state.device.settings.latitude,43.6532123456789);assert.equal(f.state.device.settings.longitude,-79.3832123456789);assert.equal(f.state.device.settings.method,'muslim_world_league');assert.equal(f.state.device.settings.asr_method,'standard');assert.equal(f.state.device.settings.high_latitude,'auto');assert.equal(f.state.device.settings.offsets.asr,0);assert.deepEqual(f.state.device.settings.timezone_rules,rules);
   await page.locator('[data-view="today"]').click();await slide(page,'volume',35);await saved(page,'volume');
   await settings(page);assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(await page.locator('#method').inputValue(),'north_america');
   await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);assert.equal(f.state.mutations,2);
   await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent==='Prayer settings saved');assert.equal(f.state.device.settings.latitude,44.4);assert.equal(f.state.device.settings.asr_method,'hanafi');assert.equal(f.state.device.settings.high_latitude,'middle_of_night');assert.equal(f.state.device.settings.offsets.asr,3);assert.equal(f.state.device.settings.volume,35);assert.equal(f.state.device.settings.enabled.asr,false);
  }finally{await o.close();}
 });
 test(`${browserName}: rapid preferences serialize writes and Stop remains actionable in Settings`,async()=>{
  const f=await fixture();f.state.device.playing=true;let release;f.state.writeWait=new Promise(r=>release=r);let requested;const first=new Promise(r=>requested=r);f.state.writeRequested=requested;
  const o=await open(f,browserName),{page}=o;
  try{
   await slide(page,'volume',40);await first;await slide(page,'volume',45);await settings(page);await page.locator('#enabled-asr').uncheck();
   assert.ok(await page.locator('#settings-stop').isEnabled());await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.querySelector('#settings-stop').hidden);
   assert.equal(f.state.posts.filter(p=>p.url==='/api/settings').length,1);release();await page.waitForFunction(()=>document.querySelector('#settings-volume-value').textContent==='45%'&&document.querySelector('#preferences-feedback').textContent==='Saved to speaker');
   assert.equal(f.state.device.settings.volume,45);assert.equal(f.state.device.settings.enabled.asr,false);assert.equal(f.state.mutations,2);assert.deepEqual(f.state.posts.filter(p=>p.url==='/api/settings').map(p=>p.payload.expected_revision),[1,2]);
  }finally{release();await o.close();}
 });
 test(`${browserName}: Stop waits for authoritative playback completion`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.stopStillPlaying=true;const o=await open(f,browserName),{page}=o;
  try{
   await page.locator('#stop').click();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.includes('waiting for playback'));
   assert.equal(await page.locator('#readiness-text').textContent(),'Playing');assert.ok(await page.locator('#stop').isEnabled());
   if(process.env.OPENATHAN_UI_CAPTURE){await page.setViewportSize({width:390,height:844});await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,`phone-stop-pending-${browserName}.png`),fullPage:true});}
   await settings(page);assert.match(await page.locator('#settings-playback-feedback').textContent(),/waiting for playback/);assert.ok(await page.locator('#settings-stop').isEnabled());
   if(process.env.OPENATHAN_UI_CAPTURE){await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,`settings-stop-pending-${browserName}.png`),fullPage:true});}
   f.state.stopStillPlaying=false;f.state.actionDrop=true;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.querySelector('#settings-playback-feedback').textContent==='Playback stopped · confirmed after readback');assert.ok(await page.locator('#settings-stop').isHidden());
  }finally{await o.close();}
 });
 test(`${browserName}: sliders show drag values and coalesce keyboard adjustments`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   await page.locator('#volume').evaluate(el=>{el.value='50';el.dispatchEvent(new Event('input',{bubbles:true}));});assert.equal(await page.locator('#volume-value').textContent(),'50%');assert.equal(f.state.mutations,0);
   await page.locator('#volume').focus();for(let i=0;i<5;i++)await page.keyboard.press('ArrowRight');await page.waitForFunction(()=>document.querySelector('#volume-feedback').textContent==='Saved to speaker');assert.equal(f.state.mutations,1);assert.equal(f.state.device.settings.volume,55);
  }finally{await o.close();}
 });
 for(const key of ['settings','display','lights','time_format']){
  test(`${browserName}: ${key} lost responses require readback before another write`,async()=>{
   const f=await fixture();f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};
   const o=await open(f,browserName),{page}=o;
   const ids={settings:'preferences',display:'screen',lights:'lights',time_format:'format'},drops={settings:'drop',display:'screenDrop',lights:'lightDrop',time_format:'timeDrop'},fails={settings:'failRead',display:'screenFailRead',lights:'lightFailRead',time_format:'timeFailRead'};
   const change=async v=>key==='time_format'?page.locator('#time-format').selectOption(String(v)):slide(page,{settings:'settings-volume',display:'screen-brightness',lights:'lights-brightness'}[key],v);
   try{
    await settings(page);f.state[drops[key]]=true;f.state[fails[key]]=true;await change(key==='time_format'?12:30);await page.waitForFunction(id=>document.querySelector('#'+id+'-feedback').textContent.includes('before another write'),ids[key]);
    if(key==='settings')assert.ok(await page.locator('#skip').isDisabled());
    const writes=f.state.posts.filter(p=>p.url===({settings:'/api/settings',display:'/api/display',lights:'/api/lights',time_format:'/api/time-format'}[key])).length;
    await change(key==='time_format'?24:31);assert.equal(f.state.posts.filter(p=>p.url===({settings:'/api/settings',display:'/api/display',lights:'/api/lights',time_format:'/api/time-format'}[key])).length,writes);
    f.state[fails[key]]=false;await page.locator('#'+ids[key]+'-recovery button').first().click();await page.waitForFunction(id=>document.querySelector('#'+id+'-recovery button').textContent.includes('Retry'),ids[key]);
    await page.locator('#'+ids[key]+'-recovery button').first().click();await saved(page,ids[key]);
    assert.equal(key==='settings'?f.state.device.settings.volume:key==='display'?f.state.device.display.brightness_percent:key==='lights'?f.state.device.lights.settings.brightness_percent:f.state.device.time_format.hours,key==='time_format'?24:31);
   }finally{await o.close();}
  });
 }
 test(`${browserName}: conflict retains edits and retry starts from latest confirmed settings`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   f.state.device.revision++;f.state.device.settings.longitude=179.99999999999997;await slide(page,'volume',25);await page.waitForFunction(()=>document.querySelector('#volume-feedback').textContent.includes('another client'));
   assert.equal(await page.locator('#volume').inputValue(),'25');await page.locator('#volume-recovery button').first().click();await saved(page,'volume');assert.equal(f.state.device.settings.volume,25);assert.equal(f.state.device.settings.longitude,179.99999999999997);
  }finally{await o.close();}
 });
 test(`${browserName}: using saved preferences preserves a separate prayer draft`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{await settings(page);await page.locator('#latitude').fill('45.2');f.state.device.revision++;await slide(page,'settings-volume',20);await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent.includes('another client'));await page.locator('#preferences-recovery button').last().click();await page.waitForFunction(()=>document.querySelector('#preferences-recovery').hidden);assert.equal(await page.locator('#settings-volume').inputValue(),'70');assert.equal(await page.locator('#latitude').inputValue(),'45.2');assert.ok(await page.locator('#discard').isVisible());}
  finally{await o.close();}
 });
 test(`${browserName}: optional hardware, test banner, applying and failures stay beside their controls`,async()=>{
  const f=await fixture();f.state.device.test_mode=true;const o=await open(f,browserName),{page}=o;
  try{
   assert.ok(await page.locator('#test-banner').isVisible());assert.match(await page.locator('#test-banner').textContent(),/isolated test settings/);await settings(page);assert.ok(await page.locator('#hardware-group').isHidden());
   f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'output_unavailable'};f.state.device.application='volume_pending';await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#hardware-group').hidden);assert.match(await page.locator('#preferences-feedback').textContent(),/applying/);assert.match(await page.locator('#screen-feedback').textContent(),/unavailable/);
   f.state.screenSaveFail=true;await slide(page,'screen-brightness',80);await page.waitForFunction(()=>document.querySelector('#screen-feedback').textContent.includes('storage'));assert.equal(await page.locator('#screen-brightness').inputValue(),'80');assert.ok(await page.locator('#screen-brightness').isDisabled());assert.ok(await page.locator('#screen-recovery').isVisible());assert.ok(await page.locator('#screen-recovery button').first().isDisabled());
  }finally{await o.close();}
 });
 test(`${browserName}: unreadable light preferences disable their controls without affecting prayers`,async()=>{
  const f=await fixture();f.state.device.lights={schema:1,supported:true,revision:0,application:'storage_fault'};const o=await open(f,browserName),{page}=o;
  try{await settings(page);assert.ok(await page.locator('#lights-enabled').isDisabled());assert.ok(await page.locator('#lights-brightness').isDisabled());assert.match(await page.locator('#lights-feedback').textContent(),/unavailable/);assert.ok(await page.locator('#enabled-asr').isEnabled());assert.equal(f.state.lightMutations,0);}
  finally{await o.close();}
 });
 test(`${browserName}: device-local format and date ignore browser timezone, and polls are quiet`,async()=>{
  const f=await fixture(),o=await open(f,browserName,{timezoneId:'Pacific/Honolulu'}),{page}=o;
  try{
   assert.equal(await page.locator('#next-time').textContent(),'15:45');assert.match(await page.locator('#device-date').textContent(),/25/);await settings(page);await page.locator('#time-format').selectOption('12');await saved(page,'format');await page.locator('[data-view="today"]').click();assert.equal(await page.locator('#next-time').textContent(),'3:45 PM');
   await page.evaluate(()=>{window.changes=0;new MutationObserver(m=>window.changes+=m.length).observe(document.querySelector('#readiness-text'),{subtree:true,childList:true,characterData:true});});await refresh(page);assert.equal(await page.evaluate(()=>window.changes),0);
  }finally{await o.close();}
 });
 for(const helper of [false,true]){
  test(`${browserName}: first run ${helper?'helper requires ready preview':'manual can finish waiting for clock'}`,async()=>{
   const f=await fixture();f.state.device.setup='incomplete';f.state.device.automatic_ready=false;f.state.device.clock_ready=false;f.state.device.schedule={state:'waiting_for_time'};
   const o=await open(f,browserName),{page}=o;
   try{
    if(helper){await page.evaluate(()=>location.hash='v=1&latitude=43.65&longitude=-79.38&source=browser&timezone=America%2FToronto');await page.waitForFunction(()=>document.querySelector('#latitude').value==='43.65');assert.equal(await page.evaluate(()=>location.hash),'');}
    else{await page.locator('#latitude').fill('43.65');await page.locator('#longitude').fill('-79.38');await page.locator('#timezone').fill('America/Toronto');}
    await page.locator('#setup-next').click();await page.locator('#method').selectOption('north_america');await page.locator('#setup-next').click();await page.waitForFunction(()=>!document.querySelector('#prayer-preview').hidden);assert.equal(f.state.mutations,0);
    if(helper){assert.ok(await page.locator('#confirm').isDisabled());f.state.device.clock_ready=true;f.state.device.schedule=initial().schedule;await page.locator('#refresh').click();await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').disabled);}
    else assert.match(await page.locator('#preview-warning').textContent(),/announcements will wait/);
    await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#settings-view').hidden);assert.equal(f.state.mutations,1);assert.equal(f.state.posts.filter(p=>p.url==='/api/activate').length,1);
   }finally{await o.close();}
  });
 }
 test(`${browserName}: pending location survives failed status/timezones and rejects unsupported zones`,async()=>{
  const f=await fixture();f.state.failRead=true;f.state.failTimezones=true;
  const browser=await ({chromium,webkit}[browserName]).launch({headless:true}),page=await browser.newPage({httpCredentials:{username:'admin',password:'browser test password'}});
  try{
   await page.goto(f.url+'/#v=1&latitude=44.4&longitude=-79.2&source=ip&timezone=Invented%2FZone&accuracy=40');await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent.includes('Timezone list unavailable'));assert.equal(await page.evaluate(()=>location.hash),'');assert.equal(f.state.mutations,0);
   f.state.failRead=false;f.state.failTimezones=false;await page.locator('#reconnect').click();await page.waitForFunction(()=>document.querySelector('#latitude').value==='44.4');assert.equal(await page.locator('#timezone').inputValue(),'');assert.match(await page.locator('#location-feedback').textContent(),/Approximate IP location.*40 km/);assert.equal(f.state.mutations,0);
  }finally{await browser.close();await f.close();}
 });
 test(`${browserName}: outdated preview cannot confirm newer drafts or remote calculation changes`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;let release;f.state.previewWait=new Promise(r=>release=r);
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.locator('#longitude').fill('-78');release();await page.waitForTimeout(100);assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#longitude').inputValue(),'-78');
   await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);f.state.device.revision++;f.state.device.settings.method='north_america';await page.locator('#refresh').click();await page.waitForFunction(()=>document.querySelector('#confirm').hidden);assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(f.state.mutations,0);
  }finally{release();await o.close();}
 });
 test(`${browserName}: narrow, desktop, enlarged-text and keyboard layouts preserve controls`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   for(const hours of [12,24]){
    f.state.device.time_format.hours=hours;f.state.device.time_format.revision++;await refresh(page);
    for(const font of ['', 'Verdana, sans-serif'])for(const width of [320,390,1280])for(const scale of [1,2]){
     await page.setViewportSize({width,height:950});await page.evaluate(({scale,font})=>{document.documentElement.style.fontSize=16*scale+'px';if(font)document.documentElement.style.setProperty('--font',font);else document.documentElement.style.removeProperty('--font');},{scale,font});
     for(const view of ['today','settings']){
      await page.locator('[data-view="'+view+'"]').click();const layout=await page.evaluate(()=>({overflow:document.documentElement.scrollWidth>innerWidth,elements:Array.from(document.querySelectorAll('main *')).filter(el=>{const r=el.getBoundingClientRect();return r.width&&((r.right>innerWidth+1)||(r.left< -1)||el.scrollWidth>el.clientWidth+1);}).map(el=>el.id||el.className||el.tagName+':'+el.textContent.trim().slice(0,30))}));assert.equal(layout.overflow,false,`${width}px ${scale}x ${view} ${hours}h ${font||'system'}: ${layout.elements.join(', ')}`);
      const small=await page.locator('button:visible,input[type="range"]:visible,select:visible').evaluateAll(els=>els.filter(el=>{const r=el.getBoundingClientRect();return r.height<44||r.width<44;}).map(el=>el.id||el.textContent));assert.deepEqual(small,[]);
      if(view==='settings'){await page.keyboard.press('Tab');await page.locator('#time-format').focus();const ring=await page.locator('#time-format').evaluate(el=>{const s=getComputedStyle(el.parentElement);return {width:s.outlineWidth,style:s.outlineStyle};});assert.deepEqual(ring,{width:'3px',style:'solid'});}
     }
    }
   }
   await page.evaluate(()=>{document.documentElement.style.fontSize='16px';document.documentElement.style.removeProperty('--font');});await page.setViewportSize({width:390,height:844});await page.locator('[data-view="today"]').click();await page.locator('#volume').focus();// Safari on macOS uses Option-Tab to include buttons in native keyboard traversal.
   await page.keyboard.press(browserName==='webkit'&&process.platform==='darwin'?'Alt+Tab':'Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),'skip');
   if(process.env.OPENATHAN_UI_CAPTURE){for(const view of ['today','settings']){await page.locator('[data-view="'+view+'"]').click();await page.screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,`${view}-390-${browserName}.png`),fullPage:true});}await page.setViewportSize({width:1280,height:900});await page.locator('[data-view="today"]').click();await page.screenshot({path:join(process.env.OPENATHAN_UI_CAPTURE,`today-desktop-${browserName}.png`),fullPage:true});}
  }finally{await o.close();}
 });
 for(const key of ['display','lights','time_format']){
  test(`${browserName}: ${key} conflict keeps edits and its independent revision`,async()=>{
   const f=await fixture();f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};
   const o=await open(f,browserName),{page}=o,id={display:'screen',lights:'lights',time_format:'format'}[key];
   try{await settings(page);f.state.device[key].revision++;
    if(key==='time_format')await page.locator('#time-format').selectOption('12');else await slide(page,key==='display'?'screen-brightness':'lights-brightness',37);
    await page.waitForFunction(id=>document.querySelector('#'+id+'-feedback').textContent.includes('another client'),id);
    await page.locator('#'+id+'-recovery button').first().click();await saved(page,id);assert.equal(f.state.device.revision,1);assert.equal(f.state.device[key].revision,3);
   }finally{await o.close();}
  });
 }
 test(`${browserName}: successive enabled-prayer edits clear acknowledged nested patches`,async()=>{
  const f=await fixture();let release;f.state.writeWait=new Promise(r=>release=r);let requested;const first=new Promise(r=>requested=r);f.state.writeRequested=requested;
  const o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#enabled-asr').uncheck();await first;await page.locator('#enabled-fajr').uncheck();release();await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent==='Saved to speaker');
   f.state.device.revision++;f.state.device.settings.volume=80;await page.locator('#refresh').click();await page.waitForFunction(()=>document.querySelector('#settings-volume-value').textContent==='80%');assert.ok(await page.locator('#preferences-recovery').isHidden());assert.equal(f.state.mutations,2);
  }finally{release();await o.close();}
 });
 test(`${browserName}: lost committed response is verified once without repeating a save`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{f.state.drop=true;await slide(page,'volume',38);await saved(page,'volume');assert.equal(f.state.mutations,1);assert.equal(f.state.device.settings.volume,38);assert.ok(await page.locator('#volume-recovery').isHidden());}
  finally{await o.close();}
 });
 test(`${browserName}: queued Skip binds the displayed occurrence and lost actions reconcile`,async()=>{
  const f=await fixture();let release;f.state.writeWait=new Promise(r=>release=r);let requested;const first=new Promise(r=>requested=r);f.state.writeRequested=requested;
  const o=await open(f,browserName),{page}=o;
  try{
   await slide(page,'volume',45);await first;await page.locator('#skip').click();f.state.device.next={...f.state.device.next,name:'Maghrib',prayer:3,...f.state.device.schedule.times[4]};release();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.includes('changed'));
   assert.equal(f.state.device.skip,undefined);assert.equal(f.state.posts.find(p=>p.url==='/api/skip').payload.occurrence.name,'Asr');
   f.state.actionDrop=true;f.state.failRead=true;await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.includes('unconfirmed'));assert.ok(await page.locator('#skip').isDisabled());
   f.state.failRead=false;await page.locator('#reconnect').click();await page.waitForFunction(()=>document.querySelector('#skip').textContent==='Restore Maghrib today'&&!document.querySelector('#skip').disabled);assert.equal(f.state.posts.filter(p=>p.url==='/api/skip').length,2);
  }finally{release();await o.close();}
 });
 test(`${browserName}: prayer-save conflict requires fresh preview and keeps browser navigation warning`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#latitude').fill('45.2');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);
   f.state.device.revision++;f.state.device.settings.method='north_america';await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent.includes('not confirmed'));assert.equal(f.state.mutations,0);
   await page.locator('#prayer-recovery button').first().click();await page.waitForFunction(()=>document.querySelector('#confirm').hidden);assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#latitude').inputValue(),'45.2');assert.equal(f.state.mutations,0);
   let dismissed=false;page.once('dialog',async d=>{dismissed=d.type()==='beforeunload';await d.dismiss();});await page.goto('about:blank',{timeout:3000}).catch(()=>{});assert.ok(dismissed);assert.ok(page.url().startsWith(f.url));
  }finally{await o.close();}
 });
 for(const useSaved of [false,true]){
  test(`${browserName}: preference conflict recovery ${useSaved?'discards':'retries'} edits and invalidates a stale prayer preview`,async()=>{
   const f=await fixture(),o=await open(f,browserName),{page}=o;
   try{
    await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);
    f.state.device.settings.method='north_america';f.state.device.revision++;
    await slide(page,'settings-volume',25);await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent.includes('another client'));
    await page.locator('#preferences-recovery button').nth(useSaved?1:0).click();await page.waitForFunction(()=>document.querySelector('#preferences-recovery').hidden);await saved(page,'preferences');
    assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(f.state.device.settings.method,'north_america');assert.equal(f.state.device.settings.volume,useSaved?70:25);
    await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent==='Prayer settings saved');assert.equal(f.state.device.settings.latitude,44.4);
   }finally{await o.close();}
  });
 }
 for(const restore of [false,true])for(const readback of [false,true]){
  test(`${browserName}: delayed ${restore?'Restore':'Skip'} ${readback?'readback':'response'} cannot reverse a confirmed Stop`,async()=>{
   const f=await fixture();f.state.device.playing=true;if(restore)f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};
   const o=await open(f,browserName),{page}=o;let release,requested,oldState,actionStarted=false;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    if(readback)await page.route('**/api/status',async route=>{if(!actionStarted){await route.continue();return;}const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});
    await page.route(restore?'**/api/cancel-skip':'**/api/skip',async route=>{
     if(restore)delete f.state.device.skip;else f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};oldState=structuredClone(f.state.device);actionStarted=true;
     if(readback){await route.fulfill({status:200,body:'{'});return;}requested();await gate;await route.fulfill({status:200,json:oldState});
    });
    await page.locator('#skip').click();await started;await page.locator('#stop').click();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent==='Playback stopped');assert.equal(f.state.device.playing,false);release();
    await page.waitForFunction(()=>!document.querySelector('#skip').disabled);assert.ok(await page.locator('#stop').isHidden());assert.notEqual(await page.locator('#readiness-text').textContent(),'Playing');assert.equal(await page.locator('#skip').textContent(),restore?'Skip Asr today':'Restore Asr today');
    await settings(page);assert.ok(await page.locator('#settings-stop').isHidden());
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: initial unreadable prayer storage is a connected device fault with independent preferences`,async()=>{
  const f=await fixture();delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.application='storage_fault';f.state.device.clock_ready=false;f.state.device.automatic_ready=false;f.state.device.scheduler_fault='durable_state_unavailable';f.state.device.schedule={state:'setup_required'};
  const o=await open(f,browserName),{page}=o;
  try{
   assert.ok(await page.locator('#connection-banner').isHidden());assert.equal(await page.locator('#readiness-text').textContent(),'Saved storage unavailable');assert.match(await page.locator('#times').textContent(),/storage/i);assert.ok(await page.locator('#volume').isDisabled());assert.equal(await page.locator('#volume-value').textContent(),'—');assert.ok(await page.locator('#skip').isDisabled());
   await settings(page);assert.ok(await page.locator('#latitude').isDisabled());assert.ok(await page.locator('#enabled-asr').isDisabled());assert.match(await page.locator('#prayer-feedback').textContent(),/storage/i);assert.ok(await page.locator('#time-format').isEnabled());await page.locator('#time-format').selectOption('12');await saved(page,'format');assert.equal(f.state.device.time_format.hours,12);assert.equal(f.state.mutations,0);
  }finally{await o.close();}
 });
 test(`${browserName}: prayer storage loss preserves drafts and Stop until saved state recovers`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);const confirmed=structuredClone(f.state.device);
   delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.application='storage_fault';f.state.device.automatic_ready=false;f.state.device.schedule={state:'setup_required'};
   await page.locator('#refresh').click();await page.waitForFunction(()=>document.querySelector('#latitude').matches(':disabled'),{},{timeout:3000});assert.ok(await page.locator('#connection-banner').isHidden());assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.ok(await page.locator('#settings-volume').isDisabled());assert.ok(await page.locator('#enabled-asr').isDisabled());assert.ok(await page.locator('#settings-stop').isEnabled());
   await page.evaluate(()=>{window.faultChanges=0;const observer=new MutationObserver(m=>window.faultChanges+=m.length);for(const id of ['volume-feedback','preferences-feedback','prayer-feedback'])observer.observe(document.getElementById(id),{subtree:true,childList:true,characterData:true});});
   const poll=page.waitForResponse(r=>r.url().endsWith('/api/status'));await page.locator('#refresh').click();await (await poll).finished();await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));assert.equal(await page.evaluate(()=>window.faultChanges),0);
   await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.querySelector('#settings-stop').hidden);
   f.state.device={...confirmed,playing:false,revision:confirmed.revision+1};f.state.device.settings.method='north_america';await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#latitude').matches(':disabled'));assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.ok(await page.locator('#confirm').isHidden());await slide(page,'settings-volume',25);await saved(page,'preferences');assert.equal(f.state.device.settings.method,'north_america');assert.equal(f.state.device.settings.latitude,confirmed.settings.latitude);
  }finally{await o.close();}
 });
 test(`${browserName}: unavailable settings during save readback retain edits without another write`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');f.state.drop=true;f.state.failRead=true;await slide(page,'settings-volume',25);await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent.includes('before another write'));
   const confirmed=structuredClone(f.state.device);delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.application='storage_fault';f.state.failRead=false;
   await page.locator('#preferences-recovery button').first().click();await page.waitForFunction(()=>document.querySelector('#settings-volume').disabled,{},{timeout:3000});assert.ok(await page.locator('#connection-banner').isHidden());assert.match(await page.locator('#preferences-feedback').textContent(),/storage/i);assert.equal(await page.locator('#settings-volume-value').textContent(),'25%');assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(f.state.mutations,1);
   f.state.device=confirmed;await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#settings-volume').disabled);await page.locator('#preferences-recovery button').first().click();await page.waitForFunction(()=>document.querySelector('#preferences-recovery').hidden);assert.equal(f.state.mutations,1);assert.equal(await page.locator('#latitude').inputValue(),'44.4');
  }finally{await o.close();}
 });
 test(`${browserName}: a preview arriving during prayer storage loss cannot be confirmed`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;let release,requested;f.state.previewWait=new Promise(r=>release=r);const started=new Promise(r=>requested=r);f.state.previewRequested=requested;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await started;delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.application='storage_fault';await page.locator('#refresh').click();await page.waitForFunction(()=>document.querySelector('#latitude').matches(':disabled'),{},{timeout:3000});const response=page.waitForResponse(r=>r.url().endsWith('/api/preview'));release();await (await response).finished();await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(f.state.mutations,0);
  }finally{release();await o.close();}
 });
 for(const keepPreference of [false,true]){
  test(`${browserName}: Discard cancels an unsent prayer confirmation ${keepPreference?'and keeps a prayer preference':'behind an independent save'}`,async()=>{
   const f=await fixture(),o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);
    await page.route('**/api/time-format',async route=>{if(route.request().method()!=='POST'){await route.continue();return;}requested();await gate;await route.continue();});
    await page.locator('#time-format').selectOption('12');await started;if(keepPreference)await page.locator('#enabled-asr').uncheck();await page.locator('#confirm').click();assert.equal(f.state.mutations,0);assert.ok(await page.locator('#discard').isEnabled());await page.locator('#discard').click();assert.equal(await page.locator('#latitude').inputValue(),'0');release();
    await page.waitForFunction(()=>document.querySelector('#format-feedback').textContent==='Saved to speaker'&&document.querySelector('#preferences-feedback').textContent==='Saved to speaker');await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
    assert.equal(f.state.device.settings.latitude,0);assert.equal(f.state.device.settings.enabled.asr,!keepPreference);assert.equal(f.state.mutations,keepPreference?1:0);assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#prayer-feedback').textContent(),'Saved prayer settings restored');
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: Discard waits for a sent prayer save, then restores the confirmed settings`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;let release,requested;f.state.writeWait=new Promise(r=>release=r);const started=new Promise(r=>requested=r);f.state.writeRequested=requested;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);await page.locator('#confirm').click();await started;assert.ok(await page.locator('#discard').isDisabled());
   await page.locator('#latitude').fill('45.2');release();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent.includes('newer edits'));assert.equal(f.state.device.settings.latitude,44.4);assert.equal(await page.locator('#latitude').inputValue(),'45.2');assert.ok(await page.locator('#discard').isEnabled());await page.locator('#discard').click();assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.equal(f.state.mutations,1);
  }finally{release();await o.close();}
 });
 test(`${browserName}: unconfirmed sent prayer changes require readback before Discard`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.querySelector('#confirm').hidden);f.state.drop=true;f.state.failRead=true;await page.locator('#confirm').click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent.includes('unconfirmed'));assert.ok(await page.locator('#discard').isDisabled());assert.equal(await page.locator('#latitude').inputValue(),'44.4');
   f.state.failRead=false;await page.locator('#prayer-recovery button').first().click();await page.waitForFunction(()=>document.querySelector('#prayer-feedback').textContent==='Prayer settings saved');assert.equal(f.state.mutations,1);assert.ok(await page.locator('#discard').isHidden());
  }finally{await o.close();}
 });
 test(`${browserName}: Discard invalidates an in-flight timetable preview`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;let release,requested;f.state.previewWait=new Promise(r=>release=r);const started=new Promise(r=>requested=r);f.state.previewRequested=requested;
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await started;await page.locator('#discard').click();const response=page.waitForResponse(r=>r.url().endsWith('/api/preview'));release();await (await response).finished();await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));assert.ok(await page.locator('#confirm').isHidden());assert.equal(await page.locator('#latitude').inputValue(),'0');assert.equal(f.state.mutations,0);
  }finally{release();await o.close();}
 });
 for(const restore of [false,true])for(const readback of [false,true]){
  test(`${browserName}: delayed Stop ${readback?'readback':'response'} retains newer ${restore?'Restore':'Skip'} state`,async()=>{
   const f=await fixture();f.state.device.playing=true;if(restore)f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};const o=await open(f,browserName),{page}=o;
   let release,requested,stopStarted=false;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    if(readback)await page.route('**/api/status',async route=>{if(!stopStarted){await route.continue();return;}const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});
    await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);stopStarted=true;if(readback){await route.fulfill({status:200,body:'{'});return;}requested();await gate;await route.fulfill({status:200,json:raw});});
    await page.locator('#stop').click();await started;await page.locator('#skip').click();await page.waitForFunction(restore=>document.querySelector('#skip').textContent===(restore?'Skip Asr today':'Restore Asr today')&&!document.querySelector('#skip').disabled,restore);release();
    await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.startsWith('Playback stopped'));assert.equal(await page.locator('#skip').textContent(),restore?'Skip Asr today':'Restore Asr today');assert.equal(await page.locator('#readiness-text').textContent(),restore?'Ready to play':'Athan skipped today');assert.equal(await page.locator('.time-row[aria-current]').count(),1);assert.equal(await page.locator('.time-row[aria-current] small').textContent(),restore?'Athan on':'Skipped');assert.ok(await page.locator('#stop').isHidden());
   }finally{release();await o.close();}
  });
 }
 for(const key of ['settings','time_format','display','lights']){
  test(`${browserName}: delayed Stop retains newer confirmed ${key} preferences`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};const o=await open(f,browserName),{page}=o;
   let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});
    await page.locator('#stop').click();await started;await settings(page);if(key==='time_format')await page.locator('#time-format').selectOption('12');else await slide(page,{settings:'settings-volume',display:'screen-brightness',lights:'lights-brightness'}[key],35);await saved(page,{settings:'preferences',time_format:'format',display:'screen',lights:'lights'}[key]);release();await page.waitForFunction(()=>document.querySelector('#settings-playback-feedback').textContent==='Playback stopped');assert.ok(await page.locator('#settings-stop').isHidden());
    if(key==='time_format'){assert.equal(await page.locator('#time-format').inputValue(),'12');await page.locator('[data-view="today"]').click();assert.equal(await page.locator('#next-time').textContent(),'3:45 PM');assert.equal(await page.locator('[data-prayer="asr"] dd').textContent(),'3:45 PM');}
    else assert.equal(await page.locator({settings:'#settings-volume-value',display:'#screen-brightness-value',lights:'#lights-brightness-value'}[key]).textContent(),'35%');
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: delayed Stop retains newer firmware check results`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.device.firmware={version:'v0.2.0',state:'current',result:'',error:''};const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#stop').click();await started;await settings(page);await page.locator('#firmware-check').click();await page.waitForFunction(()=>document.querySelector('#firmware-status').textContent.includes('available'));release();await page.waitForFunction(()=>document.querySelector('#settings-playback-feedback').textContent==='Playback stopped');assert.match(await page.locator('#firmware-status').textContent(),/available/);assert.ok(await page.locator('#settings-stop').isHidden());
  }finally{release();await o.close();}
 });
 test(`${browserName}: delayed Stop cannot hide a newer authoritative playback observation`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#stop').click();await started;
   f.state.device.playing=true;await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#skip').textContent==='Restore Asr today'&&!document.querySelector('#skip').disabled);release();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.includes('waiting for playback')&&!document.querySelector('#stop').disabled);assert.equal(await page.locator('#readiness-text').textContent(),'Playing');assert.ok(await page.locator('#stop').isVisible());
   await page.unroute('**/api/stop');await page.locator('#stop').click();await page.waitForFunction(()=>document.querySelector('#stop').hidden);assert.equal(await page.locator('#skip').textContent(),'Restore Asr today');assert.equal(f.state.device.playing,false);
  }finally{release();await o.close();}
 });
 test(`${browserName}: Stop accepts its newer response after an older preference response arrives`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let releaseSave,releaseStop,saving,stopping;const saveGate=new Promise(r=>releaseSave=r),stopGate=new Promise(r=>releaseStop=r),saveStarted=new Promise(r=>saving=r),stopStarted=new Promise(r=>stopping=r);
  try{
   await page.route('**/api/settings',async route=>{const body=route.request().postDataJSON();f.state.device.settings=body.settings;f.state.device.revision++;const raw=structuredClone(f.state.device);saving();await saveGate;await route.fulfill({status:200,json:raw});});
   await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);stopping();await stopGate;await route.fulfill({status:200,json:raw});});await slide(page,'volume',35);await saveStarted;await page.locator('#stop').click();await stopStarted;releaseSave();await saved(page,'volume');releaseStop();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent==='Playback stopped');assert.ok(await page.locator('#stop').isHidden());assert.equal(await page.locator('#volume-value').textContent(),'35%');
  }finally{releaseSave();releaseStop();await o.close();}
 });
 for(const key of ['display','lights']){
  test(`${browserName}: delayed Stop retains ${key} application feedback at the same revision`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'output_unavailable'};f.state.device.lights={schema:1,supported:true,revision:1,application:'output_unavailable',settings:{enabled:true,brightness_percent:20}};
   const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r),group=key==='display'?'screen':'lights';
   try{
    await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});
    await page.route('**/api/'+key,async route=>{if(route.request().method()!=='POST'){await route.continue();return;}f.state.device[key].application='applied';await route.fulfill({status:200,json:structuredClone(f.state.device[key])});});
    await page.locator('#stop').click();await started;await settings(page);await slide(page,key==='display'?'screen-brightness':'lights-brightness',key==='display'?50:20);await page.waitForFunction(group=>document.querySelector('#'+group+'-feedback').textContent==='Saved to speaker',group);assert.equal(f.state.device[key].revision,1);release();await page.waitForFunction(()=>document.querySelector('#settings-playback-feedback').textContent==='Playback stopped');assert.equal(await page.locator('#'+group+'-feedback').textContent(),'Saved to speaker');
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: an older Stop reply cannot confirm a newer lost Skip outcome`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#stop').click();await started;f.state.actionDrop=true;f.state.failRead=true;await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#action-feedback').textContent.includes('unconfirmed'));release();await page.waitForFunction(()=>document.querySelector('#settings-playback-feedback').textContent==='Playback stopped');
   assert.ok(await page.locator('#skip').isDisabled());assert.equal(await page.locator('#readiness-text').textContent(),'Skip state unconfirmed');assert.match(await page.locator('#action-feedback').textContent(),/Skip state unconfirmed/);assert.deepEqual(f.state.device.skip,{day:f.state.device.next.day,prayer:f.state.device.next.prayer});
   f.state.failRead=false;await refresh(page);await page.waitForFunction(()=>document.querySelector('#skip').textContent==='Restore Asr today'&&!document.querySelector('#skip').disabled);assert.equal(await page.locator('#readiness-text').textContent(),'Athan skipped today');assert.equal(f.state.posts.filter(p=>p.url==='/api/skip').length,1);
  }finally{release();await o.close();}
 });
 test(`${browserName}: later optional storage faults retain confirmed values and stay quiet until recovery`,async()=>{
  const f=await fixture();f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.time_format={schema:1,revision:3,hours:12,application:'applied'};const confirmed=structuredClone(f.state.device),o=await open(f,browserName),{page}=o;
  try{
   await settings(page);f.state.device.display={schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'};f.state.device.lights={schema:1,supported:true,revision:0,application:'storage_fault'};f.state.device.time_format={schema:1,revision:0,hours:24,application:'storage_fault'};await page.locator('#refresh').click();await page.waitForFunction(()=>['screen-brightness','lights-enabled','lights-brightness','time-format'].every(id=>document.getElementById(id).disabled),{},{timeout:3000});
   for(const group of ['screen','lights','format'])assert.match(await page.locator('#'+group+'-feedback').textContent(),/storage/i);assert.equal(await page.locator('#screen-brightness-value').textContent(),'50%');assert.equal(await page.locator('#lights-brightness-value').textContent(),'20%');assert.equal(await page.locator('#time-format').inputValue(),'12');assert.ok(await page.locator('#connection-banner').isHidden());
   await page.evaluate(()=>{window.optionalFaultChanges=0;const observer=new MutationObserver(m=>window.optionalFaultChanges+=m.length);for(const id of ['screen-feedback','lights-feedback','format-feedback'])observer.observe(document.getElementById(id),{subtree:true,childList:true,characterData:true});});const response=page.waitForResponse(r=>r.url().endsWith('/api/status'));await page.locator('#refresh').click();await (await response).finished();await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));assert.equal(await page.evaluate(()=>window.optionalFaultChanges),0);
   await page.locator('[data-view="today"]').click();assert.equal(await page.locator('#next-time').textContent(),'3:45 PM');await settings(page);f.state.device=confirmed;await page.locator('#refresh').click();await page.waitForFunction(()=>!document.getElementById('time-format').disabled);for(const id of ['screen-brightness','lights-enabled','lights-brightness'])assert.ok(await page.locator('#'+id).isEnabled());for(const group of ['screen','lights','format'])assert.equal(await page.locator('#'+group+'-feedback').textContent(),'Saved to speaker');assert.equal(f.state.posts.length,0);
  }finally{await o.close();}
 });
 test(`${browserName}: initially unreadable optional stores do not substitute fallback preferences`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'};f.state.device.lights={schema:1,supported:true,revision:0,application:'storage_fault'};f.state.device.time_format={schema:1,revision:0,hours:24,application:'storage_fault'};const o=await open(f,browserName),{page}=o;
  try{
   await settings(page);for(const id of ['screen-brightness','lights-enabled','lights-brightness','time-format'])assert.ok(await page.locator('#'+id).isDisabled());assert.equal(await page.locator('#screen-brightness-value').textContent(),'—');assert.equal(await page.locator('#lights-brightness-value').textContent(),'—');assert.equal(await page.locator('#time-format').inputValue(),'');assert.equal(await page.locator('#lights-enabled').evaluate(el=>el.indeterminate),true);for(const group of ['screen','lights','format'])assert.match(await page.locator('#'+group+'-feedback').textContent(),/storage/i);
   await slide(page,'settings-volume',35);await saved(page,'preferences');assert.equal(f.state.device.settings.volume,35);await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);assert.equal(f.state.device.playing,false);assert.ok(await page.locator('#connection-banner').isHidden());
  }finally{await o.close();}
 });
 for(const key of ['display','lights','time_format']){
  test(`${browserName}: ${key} storage fault during save readback retains edits and gates recovery`,async()=>{
   const f=await fixture();f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.time_format={schema:1,revision:3,hours:12,application:'applied'};const confirmed=structuredClone(f.state.device[key]),o=await open(f,browserName),{page}=o,path=key==='time_format'?'time-format':key,group={display:'screen',lights:'lights',time_format:'format'}[key],id={display:'screen-brightness',lights:'lights-brightness',time_format:'time-format'}[key];let attempts=0;
   try{
    await settings(page);await page.route('**/api/'+path,async route=>{if(route.request().method()!=='POST'){await route.continue();return;}attempts++;f.state.device[key]=key==='display'?{schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'}:key==='lights'?{schema:1,supported:true,revision:0,application:'storage_fault'}:{schema:1,revision:0,hours:24,application:'storage_fault'};await route.fulfill({status:503,json:{error:'Storage is unavailable'}});});
    if(key==='time_format')await page.locator('#time-format').selectOption('24');else await slide(page,id,35);await page.waitForFunction(group=>document.getElementById(group+'-feedback').textContent.includes('storage'),group,{timeout:3000});assert.ok(await page.locator('#'+id).isDisabled());assert.equal(await page.locator(key==='time_format'?'#time-format':'#'+id+'-value')[key==='time_format'?'inputValue':'textContent'](),key==='time_format'?'24':'35%');assert.ok(await page.locator('#'+group+'-recovery button').first().isDisabled());
    const response=page.waitForResponse(r=>r.url().endsWith('/api/status'));await page.locator('#refresh').click();await (await response).finished();await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));assert.equal(attempts,1);assert.ok(await page.locator('#'+id).isDisabled());
    f.state.device[key]=confirmed;await page.unroute('**/api/'+path);await page.locator('#refresh').click();await page.waitForFunction(id=>!document.getElementById(id).disabled,id);await page.locator('#'+group+'-recovery button').first().click();await page.waitForFunction(group=>document.getElementById(group+'-feedback').textContent==='Saved to speaker',group);assert.equal(key==='time_format'?f.state.device[key].hours:key==='lights'?f.state.device[key].settings.brightness_percent:f.state.device[key].brightness_percent,key==='time_format'?24:35);assert.equal(f.state.posts.filter(p=>p.url==='/api/'+path).length,1);
   }finally{await o.close();}
  });
 }
 for(const fault of [false,true]){
  test(`${browserName}: a ${fault?'faulted':'conflicted'} queued Skip allows independent saves and waits for recovery`,async()=>{
   const f=await fixture();f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};const confirmed=structuredClone(f.state.device),o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('[data-view="today"]').click();await page.route('**/api/settings',async route=>{requested();await gate;await route.fulfill({status:fault?503:409,json:{error:fault?'Storage is unavailable':'Settings changed on another client'}});});await slide(page,'volume',40);await started;await page.locator('#skip').click();
    if(fault){delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.application='storage_fault';f.state.device.automatic_ready=false;}else{f.state.device.revision++;f.state.device.settings.longitude=44;}release();await page.waitForFunction(fault=>document.getElementById('volume-feedback').textContent.includes(fault?'storage':'another client'),fault);await settings(page);
    await page.locator('#time-format').selectOption('12');await saved(page,'format');await slide(page,'screen-brightness',35);await saved(page,'screen');await slide(page,'lights-brightness',40);await saved(page,'lights');assert.equal(f.state.device.time_format.hours,12);assert.equal(f.state.device.display.brightness_percent,35);assert.equal(f.state.device.lights.settings.brightness_percent,40);assert.equal(f.state.posts.filter(p=>p.url==='/api/skip').length,0);assert.equal(await page.locator('#settings-volume-value').textContent(),'40%');assert.equal(await page.locator('#latitude').inputValue(),'44.4');
    if(fault){const healthy={...confirmed,time_format:f.state.device.time_format,display:f.state.device.display,lights:f.state.device.lights};f.state.device=healthy;await page.locator('#refresh').click();await page.waitForFunction(()=>!document.getElementById('settings-volume').disabled);}await page.unroute('**/api/settings');await page.locator('#preferences-recovery button').nth(1).click();await page.waitForFunction(()=>document.getElementById('skip').textContent==='Restore Asr today'&&!document.getElementById('skip').disabled);assert.equal(f.state.posts.filter(p=>p.url==='/api/skip').length,1);assert.equal(f.state.mutations,0);assert.equal(await page.locator('#latitude').inputValue(),'44.4');
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: an older Stop snapshot with optional faults preserves newer healthy preferences`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/stop',async route=>{f.state.device.playing=false;const raw=structuredClone(f.state.device);raw.display={schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'};raw.lights={schema:1,supported:true,revision:0,application:'storage_fault'};raw.time_format={schema:1,revision:0,hours:24,application:'storage_fault'};requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#stop').click();await started;await settings(page);await page.locator('#time-format').selectOption('12');await saved(page,'format');await slide(page,'screen-brightness',35);await saved(page,'screen');await slide(page,'lights-brightness',40);await saved(page,'lights');release();await page.waitForFunction(()=>document.getElementById('settings-playback-feedback').textContent==='Playback stopped');
   for(const id of ['screen-brightness','lights-enabled','lights-brightness','time-format'])assert.ok(await page.locator('#'+id).isEnabled());for(const group of ['screen','lights','format'])assert.equal(await page.locator('#'+group+'-feedback').textContent(),'Saved to speaker');assert.equal(await page.locator('#time-format').inputValue(),'12');assert.equal(await page.locator('#screen-brightness-value').textContent(),'35%');assert.equal(await page.locator('#lights-brightness-value').textContent(),'40%');
  }finally{release();await o.close();}
 });
 for(const key of ['display','lights','time_format']){
  test(`${browserName}: an older ${key} save reply cannot hide a newer storage fault`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.time_format={schema:1,revision:3,hours:12,application:'applied'};const o=await open(f,browserName),{page}=o,path=key==='time_format'?'time-format':key,group={display:'screen',lights:'lights',time_format:'format'}[key],id={display:'screen-brightness',lights:'lights-brightness',time_format:'time-format'}[key];let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0;
   try{
    await settings(page);await page.route('**/api/'+path,async route=>{if(route.request().method()!=='POST'){await route.continue();return;}attempts++;const raw={...structuredClone(f.state.device[key]),revision:4};if(key==='time_format')raw.hours=24;else if(key==='lights')raw.settings.brightness_percent=35;else raw.brightness_percent=35;requested();await gate;await route.fulfill({status:200,json:raw});});if(key==='time_format')await page.locator('#time-format').selectOption('24');else await slide(page,id,35);await started;
    f.state.device[key]=key==='display'?{schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'}:key==='lights'?{schema:1,supported:true,revision:0,application:'storage_fault'}:{schema:1,revision:0,hours:24,application:'storage_fault'};await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);const readback=page.waitForResponse(r=>r.url().endsWith('/api/'+path)&&r.request().method()==='GET',{timeout:3000});release();await (await readback).finished();await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
    assert.ok(await page.locator('#'+id).isDisabled());assert.match(await page.locator('#'+group+'-feedback').textContent(),/storage/i);assert.equal(await page.locator(key==='time_format'?'#time-format':'#'+id+'-value')[key==='time_format'?'inputValue':'textContent'](),key==='time_format'?'24':'35%');assert.ok(await page.locator('#'+group+'-recovery button').first().isDisabled());assert.equal(attempts,1);assert.ok(await page.locator('#connection-banner').isHidden());assert.equal(f.state.device.playing,false);
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: an older complete settings reply cannot clear newer optional storage faults`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.time_format={schema:1,revision:3,hours:12,application:'applied'};const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/settings',async route=>{f.state.device.settings=route.request().postDataJSON().settings;f.state.device.revision++;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await slide(page,'volume',35);await started;
   f.state.device.display={schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'};f.state.device.lights={schema:1,supported:true,revision:0,application:'storage_fault'};f.state.device.time_format={schema:1,revision:0,hours:24,application:'storage_fault'};await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('stop').hidden);release();await saved(page,'volume');await settings(page);
   for(const id of ['screen-brightness','lights-enabled','lights-brightness','time-format'])assert.ok(await page.locator('#'+id).isDisabled());for(const group of ['screen','lights','format'])assert.match(await page.locator('#'+group+'-feedback').textContent(),/storage/i);assert.equal(await page.locator('#time-format').inputValue(),'12');assert.equal(await page.locator('#settings-volume-value').textContent(),'35%');assert.ok(await page.locator('#settings-volume').isEnabled());assert.equal(f.state.device.playing,false);
  }finally{release();await o.close();}
 });
 for(const missing of [false,true]){
  test(`${browserName}: an older prayer save reply cannot clear newer ${missing?'unreadable':'faulted'} storage`,async()=>{
   const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested,committed;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0;
   try{
    await page.route('**/api/settings',async route=>{attempts++;f.state.device.settings=route.request().postDataJSON().settings;f.state.device.revision++;committed=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:committed});});await slide(page,'volume',35);await started;
    f.state.device.application='storage_fault';f.state.device.automatic_ready=false;if(missing){delete f.state.device.settings;f.state.device.revision=0;f.state.device.setup='storage_fault';f.state.device.schedule={state:'setup_required'};}await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('stop').hidden);release();await page.waitForFunction(()=>!writeBusy,{},{timeout:3000});
    assert.ok(await page.locator('#volume').isDisabled());assert.match(await page.locator('#volume-feedback').textContent(),/storage/i);assert.equal(await page.locator('#volume-value').textContent(),'35%');assert.ok(await page.locator('#connection-banner').isHidden());await settings(page);assert.ok(await page.locator('#enabled-asr').isDisabled());assert.ok(await page.locator('#preview').isDisabled());await page.locator('#time-format').selectOption('12');await saved(page,'format');assert.equal(attempts,1);
    f.state.device={...committed,playing:false,time_format:f.state.device.time_format};await page.locator('#refresh').click();await page.waitForFunction(()=>!document.getElementById('settings-volume').disabled);await page.locator('#preferences-recovery button').first().click();await page.waitForFunction(()=>document.getElementById('preferences-recovery').hidden);assert.equal(attempts,1);assert.equal(await page.locator('#settings-volume-value').textContent(),'35%');
   }finally{release();await o.close();}
  });
 }
 for(const prayerDraft of [false,true]){
  test(`${browserName}: an older prayer ${prayerDraft?'confirmation':'preference'} readback retains edits after a newer storage fault`,async()=>{
   const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested,committed;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0,reads=0;
   try{
    await settings(page);await page.route('**/api/settings',async route=>{attempts++;f.state.device.settings=route.request().postDataJSON().settings;f.state.device.revision++;committed=structuredClone(f.state.device);await route.fulfill({status:200,contentType:'application/json',body:'{'});});await page.route('**/api/status',async route=>{if(++reads!==1){await route.continue();return;}const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});
    if(prayerDraft){await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.getElementById('confirm').hidden);await page.locator('#confirm').click();}else await slide(page,'settings-volume',35);await started;
    f.state.device.application='storage_fault';f.state.device.automatic_ready=false;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);release();await page.waitForFunction(()=>!writeBusy,{},{timeout:3000});assert.ok(await page.locator('#connection-banner').isHidden());assert.ok(await page.locator('#settings-volume').isDisabled());assert.match(await page.locator('#preferences-feedback').textContent(),/storage/i);assert.ok(await page.locator('#confirm').isHidden());
    if(prayerDraft){assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.ok(await page.locator('#discard').isDisabled());}else assert.equal(await page.locator('#settings-volume-value').textContent(),'35%');await page.locator('#time-format').selectOption('12');await saved(page,'format');assert.equal(attempts,1);
    f.state.device={...committed,playing:false,time_format:f.state.device.time_format};await page.locator('#refresh').click();await page.waitForFunction(()=>!document.getElementById('settings-volume').disabled);await page.locator('#preferences-recovery button').first().click();await page.waitForFunction(()=>document.getElementById('preferences-recovery').hidden);assert.equal(attempts,1);if(prayerDraft){assert.equal(await page.locator('#prayer-feedback').textContent(),'Prayer settings saved');assert.equal(await page.locator('#latitude').inputValue(),'44.4');}
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: an older Skip reply retains newer prayer storage and Stop observations`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
  try{
   await page.route('**/api/skip',async route=>{const occurrence=route.request().postDataJSON().occurrence;f.state.device.skip={day:occurrence.day,prayer:occurrence.prayer};const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#skip').click();await started;f.state.device.application='storage_fault';f.state.device.automatic_ready=false;await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('stop').hidden);release();await page.waitForFunction(()=>!writeBusy,{},{timeout:3000});assert.ok(await page.locator('#volume').isDisabled());assert.match(await page.locator('#volume-feedback').textContent(),/storage/i);assert.ok(await page.locator('#connection-banner').isHidden());assert.ok(await page.locator('#stop').isHidden());assert.equal(f.state.device.playing,false);
  }finally{release();await o.close();}
 });
 for(const restore of [false,true])for(const readback of [false,true]){
  test(`${browserName}: an older ${restore?'Restore':'Skip'} ${readback?'readback':'reply'} keeps the newer prayer occurrence`,async()=>{
   const f=await fixture();f.state.device.playing=true;if(restore)f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r),path=restore?'cancel-skip':'skip';let reads=0;
   try{
    await page.route('**/api/'+path,async route=>{if(restore)delete f.state.device.skip;else f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};if(readback){await route.fulfill({status:200,contentType:'application/json',body:'{'});return;}const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});if(readback)await page.route('**/api/status',async route=>{if(++reads!==1){await route.continue();return;}const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#skip').click();await started;
    const next=f.state.device.schedule.times[4];f.state.device.next={day:20721,prayer:3,name:'Maghrib',local:next.local,utc:next.utc};delete f.state.device.skip;await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('next-name').textContent==='Maghrib');release();await page.waitForFunction(()=>!actionBusy,{},{timeout:3000});assert.equal(await page.locator('#next-name').textContent(),'Maghrib');assert.equal(await page.locator('#next-time').textContent(),'18:20');assert.equal(await page.locator('#skip').textContent(),'Skip Maghrib today');assert.match(await page.locator('#action-feedback').textContent(),/current prayer/);assert.equal(await page.locator('[aria-current="time"]').count(),1);assert.equal(await page.locator('[aria-current="time"]').getAttribute('data-prayer'),'maghrib');assert.equal(await page.locator('#readiness-text').textContent(),'Ready to play');assert.ok(await page.locator('#stop').isHidden());
   }finally{release();await o.close();}
  });
 }
 for(const waiting of [false,true]){
  test(`${browserName}: an older Skip reply retains newer ${waiting?'waiting clock':'paused volume'} readiness`,async()=>{
   const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);
   try{
    await page.route('**/api/skip',async route=>{f.state.device.skip={day:f.state.device.next.day,prayer:f.state.device.next.prayer};const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#skip').click();await started;delete f.state.device.skip;f.state.device.automatic_ready=false;if(waiting){f.state.device.clock_ready=false;delete f.state.device.next;delete f.state.device.local_date;f.state.device.schedule={state:'waiting_for_time'};}else f.state.device.application='volume_failed';await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('stop').hidden);release();await page.waitForFunction(()=>!actionBusy,{},{timeout:3000});assert.equal(await page.locator('#readiness-text').textContent(),waiting?'Waiting for time':'Automatic playback paused');assert.ok(await page.locator('#connection-banner').isHidden());if(waiting){assert.equal(await page.locator('#next-name').textContent(),'—');assert.equal(await page.locator('[aria-current="time"]').count(),0);assert.ok(await page.locator('#device-date').isHidden());assert.ok(await page.locator('#skip').isDisabled());}else assert.equal(await page.locator('#skip').textContent(),'Skip Asr today');
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: an older settings acknowledgment saves preferences while retaining the newer timetable and date`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0;
  try{
   await page.route('**/api/settings',async route=>{attempts++;f.state.device.settings=route.request().postDataJSON().settings;f.state.device.revision++;const raw=structuredClone(f.state.device);requested();await gate;await route.fulfill({status:200,json:raw});});await slide(page,'volume',35);await started;f.state.device.local_date='2026-09-26';for(const time of f.state.device.schedule.times){time.local=time.local.replace('2026-09-25','2026-09-26');time.utc+=86400;}const next=f.state.device.schedule.times[0];f.state.device.next={day:20722,prayer:0,name:'Fajr',local:next.local,utc:next.utc};await page.locator('#stop').click();await page.waitForFunction(()=>document.getElementById('next-name').textContent==='Fajr');const date=await page.locator('#device-date').textContent();release();await saved(page,'volume');assert.equal(await page.locator('#next-name').textContent(),'Fajr');assert.equal(await page.locator('#next-time').textContent(),'05:30');assert.equal(await page.locator('#skip').textContent(),'Skip Fajr today');assert.equal(await page.locator('[aria-current="time"]').getAttribute('data-prayer'),'fajr');assert.equal(await page.locator('#device-date').textContent(),date);assert.equal(await page.locator('#volume-value').textContent(),'35%');assert.equal(attempts,1);
  }finally{release();await o.close();}
 });
 for(const key of ['display','lights','time_format'])for(const recoveryRead of [false,true]){
  test(`${browserName}: stale ${key} ${recoveryRead?'recovery':'save'} readback preserves connection and active Stop`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.stopStillPlaying=true;f.state.device.display={schema:1,supported:true,revision:3,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:3,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.time_format={schema:1,revision:3,hours:24,application:'applied'};const o=await open(f,browserName),{page}=o,path=key==='time_format'?'time-format':key,group={display:'screen',lights:'lights',time_format:'format'}[key],id={display:'screen-brightness',lights:'lights-brightness',time_format:'time-format'}[key];let release,requested,committed;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0,reads=0;
   try{
    await settings(page);await page.route('**/api/'+path,async route=>{if(route.request().method()==='POST'){attempts++;const store=f.state.device[key];if(key==='time_format')store.hours=12;else if(key==='lights')store.settings.brightness_percent=35;else store.brightness_percent=35;store.revision++;committed=structuredClone(store);await route.fulfill({status:200,contentType:'application/json',body:'{'});return;}reads++;if(recoveryRead&&reads===1){await route.fulfill({status:503,json:{error:'Read unavailable'}});return;}if(reads!==(recoveryRead?2:1)){await route.continue();return;}const raw=structuredClone(f.state.device[key]);requested();await gate;await route.fulfill({status:200,json:raw});});if(key==='time_format')await page.locator('#time-format').selectOption('12');else await slide(page,id,35);
    if(recoveryRead){await page.waitForFunction(group=>document.getElementById(group+'-feedback').textContent.includes('before another write'),group);await page.locator('#refresh').click();await page.waitForFunction(()=>document.getElementById('connection-banner').hidden);await page.locator('#'+group+'-recovery button').first().click();}await started;f.state.device[key]=key==='display'?{schema:1,supported:true,revision:0,brightness_percent:75,application:'storage_fault'}:key==='lights'?{schema:1,supported:true,revision:0,application:'storage_fault'}:{schema:1,revision:0,hours:24,application:'storage_fault'};await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-playback-feedback').textContent.includes('waiting for playback'));await page.evaluate(()=>{window.staleConnectionLosses=0;const banner=document.getElementById('connection-banner');new MutationObserver(()=>{if(!banner.hidden)window.staleConnectionLosses++;}).observe(banner,{attributes:true,attributeFilter:['hidden']});});release();await page.waitForFunction(key=>!writeBusy&&!domains[key].busy,key,{timeout:3000});assert.ok(await page.locator('#connection-banner').isHidden());assert.equal(await page.evaluate(()=>window.staleConnectionLosses),0);assert.ok(await page.locator('#settings-stop').isEnabled());assert.ok(await page.locator('#'+id).isDisabled());assert.match(await page.locator('#'+group+'-feedback').textContent(),/storage/i);assert.equal(await page.locator(key==='time_format'?'#time-format':'#'+id+'-value')[key==='time_format'?'inputValue':'textContent'](),key==='time_format'?'12':'35%');assert.equal(attempts,1);
    await slide(page,'settings-volume',45);await saved(page,'preferences');assert.equal(f.state.device.settings.volume,45);f.state.device[key]=committed;await page.locator('#refresh').click();await page.waitForFunction(id=>!document.getElementById(id).disabled,id);await page.locator('#'+group+'-recovery button').first().click();await page.waitForFunction(group=>document.getElementById(group+'-recovery').hidden,group);assert.equal(attempts,1);f.state.stopStillPlaying=false;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);assert.equal(f.state.device.playing,false);
   }finally{release();await o.close();}
  });
 }
 for(const key of ['settings','display','lights','time_format'])for(const mode of ['reply-use-saved','reply-retry','readback']){
  test(`${browserName}: a stale healthy ${key} ${mode} acknowledgment retains newer preferences and user edits`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.stopStillPlaying=true;f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};const o=await open(f,browserName),{page}=o,path=key==='time_format'?'time-format':key,group={settings:'preferences',display:'screen',lights:'lights',time_format:'format'}[key],id={settings:'settings-volume',display:'screen-brightness',lights:'lights-brightness',time_format:'time-format'}[key];let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r),bodies=[];let reads=0;
   try{
    await settings(page);if(key==='settings'){await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.getElementById('confirm').hidden);}const intercept=async route=>{const store=key==='settings'?f.state.device:f.state.device[key];if(route.request().method()==='POST'){const body=route.request().postDataJSON();assert.equal(body.expected_revision,store.revision);bodies.push(body);if(key==='settings'||key==='lights')store.settings=body.settings;else if(key==='time_format')store.hours=body.hours;else store.brightness_percent=body.brightness_percent;store.revision++;const raw=structuredClone(store);if(bodies.length>1){await route.fulfill({status:200,json:raw});return;}if(mode==='readback'){await route.fulfill({status:200,contentType:'application/json',body:'{'});return;}requested();await gate;await route.fulfill({status:200,json:raw});return;}if(mode==='readback'&&++reads===1){const raw=structuredClone(store);requested();await gate;await route.fulfill({status:200,json:raw});return;}await route.continue();};await page.route('**/api/'+path,intercept);if(key==='settings'&&mode==='readback')await page.route('**/api/status',intercept);if(key==='time_format')await page.locator('#time-format').selectOption('12');else await slide(page,id,35);await started;
    const store=key==='settings'?f.state.device:f.state.device[key];store.revision++;if(key==='settings'){store.settings.volume=90;store.settings.latitude=45.123456789;store.settings.method='north_america';}else if(key==='lights')store.settings.brightness_percent=80;else if(key==='time_format')store.hours=24;else store.brightness_percent=80;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-playback-feedback').textContent.includes('waiting for playback'));release();await page.waitForFunction(key=>!writeBusy&&!domains[key].busy,key,{timeout:3000});
    assert.ok(await page.locator('#connection-banner').isHidden());assert.ok(await page.locator('#settings-stop').isEnabled());assert.equal(await page.locator(key==='time_format'?'#time-format':'#'+id+'-value')[key==='time_format'?'inputValue':'textContent'](),key==='time_format'?'12':'35%');assert.match(await page.locator('#'+group+'-feedback').textContent(),/another client|unconfirmed/i);assert.equal(bodies.length,1);if(key==='time_format')assert.equal(await page.locator('#next-time').textContent(),'15:45');if(key==='settings'){assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.ok(await page.locator('#confirm').isHidden());}
    if(mode==='readback'){await page.locator('#'+group+'-recovery button').first().click();await page.waitForFunction(group=>document.getElementById(group+'-recovery').textContent.includes('Use saved values'),group);}if(mode==='reply-retry'){await page.locator('#'+group+'-recovery button').first().click();await page.waitForFunction(group=>document.getElementById(group+'-feedback').textContent==='Saved to speaker'&&document.getElementById(group+'-recovery').hidden,group);assert.equal(bodies.length,2);assert.equal(bodies[1].expected_revision,3);}else{await page.locator('#'+group+'-recovery button').last().click();await page.waitForFunction(group=>document.getElementById(group+'-recovery').hidden,group);assert.equal(bodies.length,1);assert.equal(await page.locator(key==='time_format'?'#time-format':'#'+id+'-value')[key==='time_format'?'inputValue':'textContent'](),key==='time_format'?'24':key==='settings'?'90%':'80%');}if(key==='settings'){assert.equal(f.state.device.settings.latitude,45.123456789);assert.equal(f.state.device.settings.method,'north_america');assert.equal(await page.locator('#latitude').inputValue(),'44.4');}f.state.stopStillPlaying=false;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);
   }finally{release();await o.close();}
  });
 }
 for(const key of ['display','lights']){
  test(`${browserName}: a stale healthy ${key} reply reads back newer application state at the same revision`,async()=>{
   const f=await fixture();f.state.device.playing=true;f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};const o=await open(f,browserName),{page}=o,group=key==='display'?'screen':'lights',id=key==='display'?'screen-brightness':'lights-brightness';let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let attempts=0,reads=0;
   try{
    await settings(page);await page.route('**/api/'+key,async route=>{if(route.request().method()==='GET'){reads++;await route.continue();return;}attempts++;const store=f.state.device[key];if(key==='display')store.brightness_percent=35;else store.settings.brightness_percent=35;store.revision++;const raw=structuredClone(store);requested();await gate;await route.fulfill({status:200,json:raw});});await slide(page,id,35);await started;f.state.device[key].application='output_unavailable';await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);release();await page.waitForFunction(()=>!writeBusy,{},{timeout:3000});assert.match(await page.locator('#'+group+'-feedback').textContent(),/unavailable/i);assert.equal(await page.locator('#'+id+'-value').textContent(),'35%');assert.equal(attempts,1);assert.equal(reads,1);assert.ok(await page.locator('#connection-banner').isHidden());
   }finally{release();await o.close();}
  });
 }
 test(`${browserName}: stale healthy time-format recovery retains edits and never flashes an obsolete format`,async()=>{
  const f=await fixture();f.state.device.playing=true;f.state.stopStillPlaying=true;f.state.timeDrop=true;f.state.timeFailRead=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r);let reads=0;
  try{
   await settings(page);await page.locator('#time-format').selectOption('12');await page.waitForFunction(()=>document.getElementById('format-feedback').textContent.includes('before another write'));f.state.timeFailRead=false;await page.locator('#refresh').click();await page.waitForFunction(()=>document.getElementById('connection-banner').hidden);await page.route('**/api/time-format',async route=>{if(route.request().method()!=='GET'||++reads!==1){await route.continue();return;}const raw=structuredClone(f.state.device.time_format);requested();await gate;await route.fulfill({status:200,json:raw});});await page.locator('#format-recovery button').first().click();await started;f.state.device.time_format.hours=24;f.state.device.time_format.revision++;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-playback-feedback').textContent.includes('waiting for playback'));await page.evaluate(()=>{window.obsoleteFormatFlashes=0;const time=document.getElementById('next-time');new MutationObserver(()=>{if(time.textContent.includes('PM'))window.obsoleteFormatFlashes++;}).observe(time,{childList:true,characterData:true,subtree:true});});release();await page.waitForFunction(()=>!domains.time_format.busy,{},{timeout:3000});assert.equal(await page.locator('#time-format').inputValue(),'12');assert.equal(await page.locator('#next-time').textContent(),'15:45');assert.equal(await page.evaluate(()=>window.obsoleteFormatFlashes),0);assert.ok(await page.locator('#connection-banner').isHidden());assert.ok(await page.locator('#settings-stop').isEnabled());await page.waitForFunction(()=>document.getElementById('format-recovery').textContent.includes('Use saved values'));await page.locator('#format-recovery button').last().click();await page.waitForFunction(()=>document.getElementById('format-recovery').hidden);assert.equal(await page.locator('#time-format').inputValue(),'24');assert.equal(f.state.timeMutations,1);
  }finally{release();await o.close();}
 });
 test(`${browserName}: a stale healthy prayer confirmation keeps its draft until fresh review`,async()=>{
  const f=await fixture();f.state.device.playing=true;const o=await open(f,browserName),{page}=o;let release,requested;const gate=new Promise(r=>release=r),started=new Promise(r=>requested=r),bodies=[];
  try{
   await settings(page);await page.locator('#latitude').fill('44.4');await page.locator('#preview').click();await page.waitForFunction(()=>!document.getElementById('confirm').hidden);await page.route('**/api/settings',async route=>{const body=route.request().postDataJSON();assert.equal(body.expected_revision,f.state.device.revision);bodies.push(body);f.state.device.settings=body.settings;f.state.device.revision++;const raw=structuredClone(f.state.device);if(bodies.length===1){requested();await gate;}await route.fulfill({status:200,json:raw});});await page.locator('#confirm').click();await started;f.state.device.settings.latitude=45.123456789;f.state.device.revision++;await page.locator('#settings-stop').click();await page.waitForFunction(()=>document.getElementById('settings-stop').hidden);release();await page.waitForFunction(()=>!writeBusy,{},{timeout:3000});assert.equal(await page.locator('#latitude').inputValue(),'44.4');assert.ok(await page.locator('#confirm').isHidden());assert.match(await page.locator('#prayer-feedback').textContent(),/not confirmed|draft/i);assert.equal(bodies.length,1);await page.locator('#prayer-recovery button').first().click();await page.waitForFunction(()=>!document.getElementById('preview').disabled);assert.equal(bodies.length,1);await page.locator('#preview').click();await page.waitForFunction(()=>!document.getElementById('confirm').hidden);await page.locator('#confirm').click();await page.waitForFunction(()=>document.getElementById('prayer-feedback').textContent==='Prayer settings saved');assert.equal(bodies.length,2);assert.equal(bodies[1].expected_revision,3);assert.equal(f.state.device.settings.latitude,44.4);
  }finally{release();await o.close();}
 });
 test(`${browserName}: text and controls meet the contrast requirements`,async()=>{
  const f=await fixture(),o=await open(f,browserName),{page}=o;
  try{
   const colors=await page.evaluate(()=>{const c=getComputedStyle(document.documentElement);return Object.fromEntries(['bg','fg','muted','amber','upcoming-bg','border','surface','good','bad'].map(k=>[k,c.getPropertyValue('--'+k).trim()]));});
   const lum=h=>{const v=h.slice(1).match(/../g).map(x=>parseInt(x,16)/255).map(x=>x<=.04045?x/12.92:((x+.055)/1.055)**2.4);return .2126*v[0]+.7152*v[1]+.0722*v[2];},ratio=(a,b)=>(Math.max(lum(a),lum(b))+.05)/(Math.min(lum(a),lum(b))+.05);
   for(const text of ['fg','muted','amber','good','bad'])for(const bg of ['bg','surface','upcoming-bg'])assert.ok(ratio(colors[text],colors[bg])>=4.5,`${text} on ${bg}`);
   assert.ok(ratio(colors.border,colors.surface)>=3);assert.ok(ratio(colors.bg,colors.amber)>=4.5);
  }finally{await o.close();}
 });

}

if(process.env.OPENATHAN_UI_CAPTURE)test('production UI visual evidence with simulated APIs',async()=>{
 const f=await fixture(),o=await open(f,'chromium'),{page}=o,out=process.env.OPENATHAN_UI_CAPTURE;
 const capture=async name=>{await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:join(out,name+'.png'),fullPage:true});};
 try{
  for(const width of [320,390,1280]){await page.setViewportSize({width,height:width===1280?900:844});await capture(width===1280?'desktop':width===390?'mobile':'phone-320');}
  await page.setViewportSize({width:390,height:844});await page.locator('#skip').click();await page.waitForFunction(()=>document.querySelector('#readiness-text').textContent==='Athan skipped today');await capture('phone-skipped');
  f.state.device.playing=true;await refresh(page);await page.waitForFunction(()=>!document.querySelector('#stop').hidden);await capture('phone-playing');await settings(page);await capture('settings-playing');
  f.state.failRead=true;await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#connection-banner').hidden);await page.locator('[data-view="today"]').click();await capture('phone-disconnected');f.state.failRead=false;await page.locator('#reconnect').click();
  f.state.device.playing=false;await settings(page);await page.locator('#refresh').click();f.state.device.revision++;await slide(page,'settings-volume',30);await page.waitForFunction(()=>document.querySelector('#preferences-feedback').textContent.includes('another client'));await capture('settings-save-failure');await page.locator('#preferences-recovery button').last().click();await page.waitForFunction(()=>document.querySelector('#preferences-recovery').hidden);
  await capture('settings');f.state.device.display={schema:1,supported:true,revision:1,brightness_percent:50,application:'applied'};f.state.device.lights={schema:1,supported:true,revision:1,application:'applied',settings:{enabled:true,brightness_percent:20}};f.state.device.firmware={version:'v0.2.0',state:'available',available:{version:'v0.3.0'},result:'',error:''};await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#hardware-group').hidden);await capture('settings-capabilities');await page.setViewportSize({width:1280,height:900});await capture('settings-desktop');await page.setViewportSize({width:320,height:900});await page.evaluate(()=>document.documentElement.style.fontSize='32px');await capture('settings-enlarged');
 }finally{await o.close();}
 const setup=await fixture();setup.state.device.setup='incomplete';setup.state.device.automatic_ready=false;const q=await open(setup,'chromium',{viewport:{width:390,height:844}});
 try{const capture=async name=>{await q.page.evaluate(()=>scrollTo(0,0));await q.page.screenshot({path:join(out,name+'.png'),fullPage:true});};await capture('setup-location');await q.page.locator('#latitude').fill('43.65');await q.page.locator('#longitude').fill('-79.38');await q.page.locator('#timezone').fill('America/Toronto');await q.page.locator('#setup-next').click();await capture('setup-calculation');await q.page.locator('#method').selectOption('north_america');await q.page.locator('#setup-next').click();await q.page.waitForFunction(()=>!document.querySelector('#prayer-preview').hidden);await capture('setup-review');}
 finally{await q.close();}
});

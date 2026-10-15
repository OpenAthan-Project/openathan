/* Real bootstrap assets with simulated setup endpoints. Device/AP security and
 * durable credential policies have separate production C++ tests. */
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createServer}=require('node:http');
const {readFile,mkdir}=require('node:fs/promises');
const {join}=require('node:path');
const playwright=require(require.resolve('playwright',{paths:[join(__dirname,'../web/device-ui')]}));
const speakerPassword='speaker pass 123';
const wifiPassword='home pass 123';
async function fixture(){
 const state={status:{state:'ready',setup_ssid:'OpenAthan-Setup-a1b2c3',speaker_url:'http://openathan-a1b2c3.local/',password_set:false,error:''},
  networks:[{ssid:'Home Wi-Fi',rssi:-42,secured:true},{ssid:'Open guest network',rssi:-60,secured:false}],scans:0,reads:0,connects:[],requests:[]};
 const server=createServer(async(req,res)=>{
  state.requests.push(req.url);
  const send=(code,data)=>{res.writeHead(code,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));};
  const assets={'/setup/':['index.html','text/html'],'/setup/style.css':['style.css','text/css'],'/setup/app.js':['app.js','text/javascript']};
  if(assets[req.url]){
   const [name,type]=assets[req.url];res.writeHead(200,{'Content-Type':type,'Content-Security-Policy':"default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'"});
   res.end(await readFile(join(__dirname,'../web/device-ui/setup',name)));return;
  }
  if(req.method==='GET'&&req.url==='/setup/status'){
   if(state.dropStatus){req.socket.destroy();return;}send(200,state.status);return;
  }
  if(req.method==='GET'&&req.url==='/setup/networks'){
   state.reads++;state.networkStarted?.();if(state.networkWait)await state.networkWait;
   if(state.scanFailure){send(503,{error:'Scan unavailable'});return;}
   send(200,{scanning:!!state.scanning,networks:state.networks});return;
  }
  if(req.method==='POST'){
   let body='';for await(const chunk of req)body+=chunk;
   if(req.headers.origin!==`http://${req.headers.host}`||!req.headers['content-type']?.startsWith('application/json')){send(403,{error:'Same-origin JSON required'});return;}
   const payload=JSON.parse(body);
   if(req.url==='/setup/scan'){state.scans++;send(202,{scanning:true});return;}
   if(req.url==='/setup/connect'){
    state.connects.push(payload);
    if(state.connectCode){send(state.connectCode,{error:state.connectError||'The speaker could not complete setup.'});return;}
    state.status.password_set=true;state.status.state='connecting';
    if(state.dropConnect){state.dropStatus=true;res.writeHead(202,{'Content-Type':'application/json'});res.end('{');return;}
    send(202,{state:'connecting',speaker_url:state.status.speaker_url});return;
   }
  }
  send(404,{error:'Not found'});
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 return {state,url:`http://127.0.0.1:${server.address().port}/setup/`,close:()=>new Promise(resolve=>server.close(resolve))};
}
async function open(t,browserName,configure){
 const f=await fixture();if(configure)configure(f.state);
 const browser=await playwright[browserName].launch({headless:true});
 const context=await browser.newContext({viewport:{width:390,height:844}});
 const page=await context.newPage();
 page.setDefaultTimeout(10000);
 t.after(async()=>{await context.close();await browser.close();await f.close();});
 await page.goto(f.url);return {...f,page,context};
}
async function password(page,existing=false){
 await page.locator('#password-step').waitFor({state:'visible'});
 await page.locator('#speaker-password').fill(speakerPassword);
 if(!existing)await page.locator('#confirm-password').fill(speakerPassword);
 await page.getByRole('button',{name:'Continue to Wi-Fi'}).click();
 await page.locator('#network-step').waitFor({state:'visible'});
}
async function network(page,manual=false){
 if(manual){await page.locator('#manual-toggle').click();await page.locator('#manual-ssid').fill('Hidden home');}
 else{await page.locator('#network option[value="Home Wi-Fi"]').waitFor({state:'attached'});await page.locator('#network').selectOption('Home Wi-Fi');}
 await page.locator('#wifi-password').fill(wifiPassword);
 await page.getByRole('button',{name:'Review connection'}).click();
 await page.locator('#connect-step').waitFor({state:'visible'});
}
async function capture(page,name){
 if(!process.env.OPENATHAN_SETUP_CAPTURE_DIR)return;
 await mkdir(process.env.OPENATHAN_SETUP_CAPTURE_DIR,{recursive:true});
 await page.evaluate(()=>window.scrollTo(0,0));
 await page.screenshot({path:join(process.env.OPENATHAN_SETUP_CAPTURE_DIR,name+'.png'),fullPage:true});
}
for(const browserName of (process.env.OPENATHAN_TEST_BROWSERS||'chromium,webkit').split(',')){
 test(`${browserName}: phone setup shows handoff before connecting and clears secrets`,async t=>{
  const {page,state}=await open(t,browserName);
  await page.locator('#password-step').waitFor({state:'visible'});
  if(browserName==='chromium'){
   await capture(page,'setup-password-phone');await page.setViewportSize({width:1100,height:850});await capture(page,'setup-password-desktop');await page.setViewportSize({width:390,height:844});
  }
  await password(page);
  assert.ok(await page.locator('#network option[value="Open guest network"]').isDisabled());
  if(browserName==='chromium')await capture(page,'setup-wifi-phone');
  await network(page);
  assert.equal(state.connects.length,0);
  assert.equal(await page.locator('#speaker-link').getAttribute('href'),'http://openathan-a1b2c3.local/');
  assert.match(await page.locator('#connect-step').innerText(),/Join Home Wi-Fi/);
  if(browserName==='chromium'){
   await capture(page,'setup-connect-phone');
   if(process.env.OPENATHAN_SETUP_CAPTURE_DIR){
    for(const width of [320,390,768,1280]){
     await page.setViewportSize({width,height:850});
     assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
     await capture(page,'setup-connect-'+width);
    }
    await page.setViewportSize({width:390,height:844});
   }
  }
  await page.locator('#connect').click();await page.waitForFunction(()=>document.getElementById('connect-feedback').textContent.includes('trying to join'));
  assert.deepEqual(state.connects,[{password:speakerPassword,ssid:'Home Wi-Fi',wifi_password:wifiPassword}]);
  for(const id of ['speaker-password','confirm-password','wifi-password'])assert.equal(await page.locator('#'+id).inputValue(),'');
  assert.equal(await page.locator('#connect-actions').isHidden(),true);
  assert.equal(await page.evaluate(()=>localStorage.length+sessionStorage.length),0);
  assert.ok(state.requests.every(path=>path.startsWith('/setup/')));
 });
 test(`${browserName}: hidden Wi-Fi remains usable when network scan fails`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.scanFailure=true;});
  await password(page);await page.waitForFunction(()=>document.getElementById('scan-feedback').textContent.includes('Could not read'));
  await network(page,true);assert.equal(await page.locator('#home-name').textContent(),'Hidden home');
  await page.locator('#connect').click();await page.waitForFunction(()=>document.getElementById('connect-feedback').textContent.includes('trying to join'));
  assert.equal(state.connects[0].ssid,'Hidden home');
 });
 test(`${browserName}: existing speaker password is verified and wrong password is not replaced`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.status.password_set=true;state.connectCode=403;state.connectError='Speaker password did not match.';});
  await page.locator('#password-step').waitFor({state:'visible'});
  assert.equal(await page.locator('#confirm-field').isHidden(),true);
  assert.equal(await page.locator('#speaker-password').getAttribute('autocomplete'),'current-password');
  await password(page,true);await network(page);await page.locator('#connect').click();
  await page.waitForFunction(()=>document.getElementById('page-feedback').textContent.includes('did not match'));
  assert.equal(state.connects.length,1);assert.ok(await page.locator('#password-step').isVisible());
  assert.equal(await page.locator('#speaker-password').inputValue(),'');
  assert.equal(await page.locator('#confirm-field').isHidden(),true);
 });
 for(const [code,message] of [[409,'Wait for the current scan or connection attempt to finish'],[429,'Too many password attempts. Wait one minute and try again']]){
  test(`${browserName}: definite ${code} rejection preserves recovery message and requires deliberate retry`,async t=>{
   const {page,state}=await open(t,browserName,state=>{state.status.password_set=true;state.connectCode=code;state.connectError=message;});
   await password(page,true);await network(page);await page.locator('#connect').click();
   await page.waitForFunction(message=>!document.getElementById('password-step').hidden&&document.getElementById('page-feedback').textContent.includes(message),message);
   assert.equal(state.connects.length,1);assert.equal(await page.locator('#speaker-password').inputValue(),'');
   assert.equal(await page.locator('#wifi-password').inputValue(),'');assert.ok(await page.locator('#confirm-field').isHidden());
   assert.match(await page.locator('#page-feedback').innerText(),/Enter your passwords again/);
   await page.clock.install();await page.clock.fastForward(60001);assert.equal(state.connects.length,1);
   state.connectCode=0;await password(page,true);await network(page);await page.locator('#connect').click();
   await page.waitForFunction(()=>document.getElementById('connect-feedback').textContent.includes('trying to join'));
   assert.equal(state.connects.length,2);
  });
 }
 test(`${browserName}: validates ASCII password, confirmation, Wi-Fi password and UTF-8 SSID bounds`,async t=>{
  const {page,state}=await open(t,browserName);
  await page.locator('#password-step').waitFor({state:'visible'});
  await page.locator('#speaker-password').fill('é'.repeat(12));await page.locator('#confirm-password').fill('é'.repeat(12));
  await page.getByRole('button',{name:'Continue to Wi-Fi'}).click();assert.ok(await page.locator('#password-step').isVisible());
  assert.equal(await page.locator('#speaker-password').evaluate(el=>el.validity.valid),false);
  await page.locator('#speaker-password').fill(speakerPassword);await page.locator('#confirm-password').fill('different password');
  await page.getByRole('button',{name:'Continue to Wi-Fi'}).click();assert.equal(await page.locator('#confirm-password').evaluate(el=>el.validity.valid),false);
  await page.locator('#speaker-password').fill('different password');await page.getByRole('button',{name:'Continue to Wi-Fi'}).click();
  await page.locator('#network-step').waitFor({state:'visible'});await page.locator('#manual-toggle').click();
  await page.locator('#manual-ssid').fill('é'.repeat(17));await page.locator('#wifi-password').fill('short');
  await page.getByRole('button',{name:'Review connection'}).click();assert.ok(await page.locator('#network-step').isVisible());
  assert.equal(await page.locator('#manual-ssid').evaluate(el=>el.validity.valid),false);
  await page.locator('#manual-ssid').fill('Hidden home');await page.getByRole('button',{name:'Review connection'}).click();
  assert.equal(await page.locator('#wifi-password').evaluate(el=>el.validity.valid),false);
  await page.locator('#wifi-password').fill('a'.repeat(64));await page.getByRole('button',{name:'Review connection'}).click();
  assert.ok(await page.locator('#connect-step').isVisible());assert.equal(state.connects.length,0);
 });
 test(`${browserName}: confirmed failed Wi-Fi permits a new attempt with existing password`,async t=>{
  const {page,state}=await open(t,browserName);await password(page);await network(page);await page.locator('#connect').click();
  await page.waitForFunction(()=>document.getElementById('connect-feedback').textContent.includes('trying to join'));
  state.status.state='failed';state.status.error='Could not join the selected network.';
  await page.locator('#check-connection').click();await page.waitForFunction(()=>!document.getElementById('password-step').hidden);
  assert.equal(await page.locator('#confirm-field').isHidden(),true);assert.match(await page.locator('#page-feedback').textContent(),/Could not join/);
  assert.equal(state.connects.length,1);
 });
 test(`${browserName}: lost write response never claims success or repeats connection write`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.dropConnect=true;});
  await password(page);await network(page);await page.locator('#connect').click();
  await page.waitForFunction(()=>document.getElementById('connect-feedback').textContent.includes('cannot confirm'));
  assert.equal(state.connects.length,1);assert.ok(await page.locator('#connect-actions').isHidden());
  assert.match(await page.locator('#connection-recovery').innerText(),/OpenAthan-Setup-a1b2c3/);
  assert.equal(await page.locator('#wifi-password').inputValue(),'');
  await page.locator('#check-connection').click();await page.waitForFunction(()=>!document.getElementById('check-connection').disabled);
  assert.equal(state.connects.length,1);assert.match(await page.locator('#connect-feedback').innerText(),/cannot confirm/);
 });
 test(`${browserName}: storage fault blocks setup and provides USB recovery`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.status.state='storage_fault';});
  await page.locator('#unavailable-step').waitFor({state:'visible'});
  assert.match(await page.locator('#unavailable-step').innerText(),/USB/);
  assert.ok(await page.locator('#password-step').isHidden());assert.equal(state.connects.length,0);
 });
 test(`${browserName}: connection storage failure clears secrets and blocks retry`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.connectCode=503;state.connectError='Setup information could not be saved.';});
  await password(page);await network(page);await page.locator('#connect').click();await page.locator('#unavailable-step').waitFor({state:'visible'});
  assert.equal(state.connects.length,1);assert.equal(await page.locator('#speaker-password').inputValue(),'');
  assert.equal(await page.locator('#wifi-password').inputValue(),'');assert.ok(await page.locator('#connect-step').isHidden());
 });
 test(`${browserName}: late scanning response cannot restart polling after review`,async t=>{
  let release;const wait=new Promise(resolve=>{release=resolve;});
  let started;const networkStarted=new Promise(resolve=>{started=resolve;});
  const {page,state}=await open(t,browserName,state=>{state.networkWait=wait;state.networkStarted=started;state.scanning=true;});
  await password(page);await networkStarted;
  await network(page,true);await page.clock.install();
  const response=page.waitForResponse(response=>response.url().endsWith('/setup/networks'));
  release();await response;await page.clock.runFor(1100);
  assert.equal(state.reads,1);assert.ok(await page.locator('#connect-step').isVisible());
 });
 test(`${browserName}: server names are text and a nonlocal speaker URL is rejected`,async t=>{
  const {page,state}=await open(t,browserName,state=>{state.status.setup_ssid='<img src=x onerror=alert(1)>';state.networks=[{ssid:'<img src=x onerror=alert(1)>',rssi:-40,secured:true}];});
  await password(page);await page.waitForFunction(()=>document.getElementById('network').options.length>1);
  assert.match(await page.locator('#connection').textContent(),/<img/);assert.equal(await page.locator('img').count(),0);
  assert.equal(await page.locator('#network option').last().textContent(),'<img src=x onerror=alert(1)>');
  state.status.speaker_url='javascript:alert(1)';await page.reload();await page.locator('#retry-status').waitFor({state:'visible'});
  assert.ok(await page.locator('#password-step').isHidden());assert.equal(await page.locator('#speaker-link').getAttribute('href'),null);
 });
 test(`${browserName}: keyboard navigation and narrow layouts work without browser storage`,async t=>{
  const f=await fixture();const browser=await playwright[browserName].launch({headless:true});
  const context=await browser.newContext({viewport:{width:320,height:780}});
  await context.addInitScript(()=>{for(const name of ['localStorage','sessionStorage'])Object.defineProperty(window,name,{get(){throw new Error('Unavailable in captive browser');}});});
  const page=await context.newPage();page.setDefaultTimeout(10000);t.after(async()=>{await context.close();await browser.close();await f.close();});
  await page.goto(f.url);await page.locator('#password-step').waitFor({state:'visible'});
  await page.keyboard.press(browserName==='webkit'?'Alt+Tab':'Tab');assert.ok(await page.locator('.skip-link').evaluate(el=>el===document.activeElement));
  await page.locator('#speaker-password').focus();await page.keyboard.type(speakerPassword);await page.keyboard.press('Tab');await page.keyboard.type(speakerPassword);await page.keyboard.press('Enter');
  await page.locator('#network-step').waitFor({state:'visible'});
  assert.ok(await page.locator('#network-heading').evaluate(el=>el===document.activeElement));
  await page.locator('#network option[value="Home Wi-Fi"]').waitFor({state:'attached'});await page.locator('#network').selectOption('Home Wi-Fi');
  await page.locator('#wifi-password').focus();await page.keyboard.type(wifiPassword);await page.keyboard.press('Enter');
  await page.locator('#connect-step').waitFor({state:'visible'});
  assert.ok(await page.locator('#connect-heading').evaluate(el=>el===document.activeElement));
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  await page.evaluate(()=>document.documentElement.style.fontSize='24px');
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  await page.locator('.browser-help summary').click();assert.match(await page.locator('.browser-help').innerText(),/Safari or Chrome/);
 });
}

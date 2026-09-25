/* Browser contract tests with a simulated device. Production settings/API,
 * activation, and Digest validation have separate C++ tests. */
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createServer}=require('node:http');
const {readFile}=require('node:fs/promises');
const {join}=require('node:path');
const {createHash,randomBytes}=require('node:crypto');
const {chromium,webkit}=require(require.resolve('playwright',{paths:[join(__dirname,'../web/device-ui')]}));
const md5=(s)=>createHash('md5').update(s).digest('hex');
const rules={standard_offset:0,daylight_offset:0,start:{type:0,time_seconds:0,day:0,month:0,week:0,day_of_week:0},end:{type:0,time_seconds:0,day:0,month:0,week:0,day_of_week:0}};
function initial(){return {schema:1,revision:1,setup:'incomplete',application:'applied',automatic_ready:false,clock_ready:true,scheduler_fault:'none',playing:false,wifi_connected:true,hostname:'openathan-test.local',settings:{latitude:0,longitude:0,timezone:'UTC',timezone_rules:rules,method:'muslim_world_league',asr_method:'standard',high_latitude:'auto',volume:70,offsets:{fajr:0,sunrise:0,dhuhr:0,asr:0,maghrib:0,isha:0},enabled:{fajr:true,dhuhr:true,asr:true,maghrib:true,isha:true}},schedule:{state:'ready',times:[{name:'Fajr',local:'2026-09-25 05:30'},{name:'Dhuhr',local:'2026-09-25 12:30'}],conflicts:[]}};}
async function fixture(){
  const state={device:initial(),mutations:0,drop:false,failRead:false,posts:[],authenticated:0};
  const nonce=randomBytes(24).toString('hex'),realm='OpenAthan-test';
  const server=createServer(async(req,res)=>{
    const params={};for(const m of (req.headers.authorization||'').matchAll(/(\w+)=(?:"([^"]*)"|([^, ]+))/g))params[m[1]]=m[2]??m[3];
    const ha1=md5(`admin:${realm}:browser test password`);
    const expected=md5(`${ha1}:${nonce}:${params.nc}:${params.cnonce}:auth:${md5(`${req.method}:${req.url}`)}`);
    if(params.username!=='admin'||params.nonce!==nonce||params.response!==expected){res.writeHead(401,{'WWW-Authenticate':`Digest realm="${realm}", nonce="${nonce}", algorithm=MD5, qop="auth"`});res.end();return;}
    state.authenticated++;
    if(!req.url.startsWith('/api/')){
      const files={'/':['index.html','text/html'],'/app.js':['app.js','text/javascript'],'/style.css':['style.css','text/css']};
      if(!files[req.url]){res.writeHead(404);res.end();return;}
      res.writeHead(200,{'Content-Type':files[req.url][1],'Content-Security-Policy':"default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'"});res.end(await readFile(join(__dirname,'../web/device-ui',files[req.url][0])));return;
    }
    const send=(code,data)=>{res.writeHead(code,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));};
    if(req.method==='GET'){if(state.failRead && req.url==='/api/status'){send(503,{error:'Status unavailable'});return;}send(200,req.url==='/api/timezones'?{names:['UTC','America/Toronto']}:state.device);return;}
    let body='';for await(const data of req)body+=data;
    const payload=JSON.parse(body);state.posts.push({url:req.url,payload,origin:req.headers.origin});
    if(req.headers.origin!==`http://${req.headers.host}`){send(403,{error:'Same-origin JSON required'});return;}
    if(req.url==='/api/preview'){send(200,state.device.clock_ready?state.device.schedule:{state:'waiting_for_time'});return;}
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
  return {state,url:`http://127.0.0.1:${server.address().port}`,close:()=>new Promise(resolve=>{server.closeAllConnections();server.close(resolve);})};
}
for(const browserName of (process.env.OPENATHAN_TEST_BROWSERS||'chromium').split(',')){
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

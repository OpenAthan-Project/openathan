"use strict";
const $ = (id) => document.getElementById(id);
const prayers = ["fajr", "dhuhr", "asr", "maghrib", "isha"];
const events = ["fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"];
const methods = {muslim_world_league:"Muslim World League",egyptian:"Egyptian",karachi:"Karachi",umm_al_qura:"Umm al-Qura",dubai:"Dubai",moonsighting_committee:"Moonsighting Committee",north_america:"North America (ISNA)",kuwait:"Kuwait",qatar:"Qatar",singapore:"Singapore",tehran:"Tehran",turkey:"Turkey"};
const faults = {"invalid settings":"Review the location, timezone and calculation settings, then save again","invalid prayer schedule":"These settings cannot produce a valid schedule. Review the location, method and offsets","durable state unavailable":"Saved data could not be read or written. Restart the device; do not erase its storage","audio unavailable":"Audio is unavailable. Check the device power and compatible audio installation","playback request rejected":"Audio could not start. Check the device power and audio installation"};
let snapshot, editing, dirty = false, busy = false, loading = false, generation = 0;
let timeSnapshot, timeEditing, timeDirty=false, timeUncertain=false, previewSchedule;
let screenSnapshot, screenEditing, screenDirty=false, screenUncertain=false;
let lightSnapshot, lightEditing, lightDirty=false, lightUncertain=false;
let locationProposal, locationNeedsPreview=false, settingsVersion=0;
const supportedZones=new Set();
let timezonesReady=false, timezonesLoading=false;
const title = (s) => s[0].toUpperCase()+s.slice(1);
function message(text, error=false) { $("message").textContent=text; $("message").classList.toggle("error",error); }
function lightMessage(text,error=false) { $("lights-message").textContent=text;$("lights-message").classList.toggle("error",error); }
function incomingLocation() {
  if(!location.hash.startsWith("#v="))return null;
  const hash=location.hash;
  history.replaceState(null,"",location.pathname+location.search);
  if(hash.length>512)return {error:true};
  const params=new URLSearchParams(hash.slice(1));
  const allowed=["v","latitude","longitude","source","timezone","accuracy"];
  if([...params.keys()].some(key=>!allowed.includes(key)) ||
     ["v","latitude","longitude","source"].some(key=>params.getAll(key).length!==1) ||
     ["timezone","accuracy"].some(key=>params.getAll(key).length>1) || params.get("v")!=="1")return {error:true};
  const latText=params.get("latitude"),lonText=params.get("longitude");
  const latitude=Number(latText),longitude=Number(lonText),source=params.get("source");
  if(!latText || !lonText || !Number.isFinite(latitude) || !Number.isFinite(longitude) ||
     latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180 ||
     !["browser","ip"].includes(source))return {error:true};
  const timezone=params.get("timezone") || "";
  if(timezone.length>64 || /[\x00-\x1f\x7f]/.test(timezone))return {error:true};
  const accuracyText=params.get("accuracy"),accuracy=accuracyText===null?undefined:Number(accuracyText);
  if(accuracyText!==null && (accuracyText==="" || !Number.isFinite(accuracy) || accuracy<0))return {error:true};
  return {latitude,longitude,timezone,source,accuracy};
}
let proposedLocation=incomingLocation();
const helperUrl=new URL("https://openathan.com/location/");
helperUrl.hash=new URLSearchParams({v:"1",device:location.origin+"/"}).toString();
$("find-location").href=helperUrl.href;
function applyLocationProposal(proposal) {
  if(!proposal)return;
  if(proposal.error){message("Location link was invalid. Enter your location manually.",true);return;}
  if(dirty && !confirm("Replace your unsaved location edits with the suggested location?")){
    message("Your unsaved edits were kept. You can enter the suggested values manually.");return;
  }
  $("latitude").value=String(proposal.latitude);$("longitude").value=String(proposal.longitude);
  const zoneSupported=!!proposal.timezone && supportedZones.has(proposal.timezone);
  $("timezone").value=zoneSupported?proposal.timezone:"";
  locationProposal=proposal;
  $("settings").dispatchEvent(new Event("input",{bubbles:true}));
  locationNeedsPreview=true;controls();
  const source=proposal.source==="ip"?"Approximate IP location":"Browser location";
  const accuracy=Number.isFinite(proposal.accuracy)
    ?proposal.source==="ip"?` Estimated radius: ${Math.ceil(proposal.accuracy)} km.`:` Reported accuracy: ${Math.ceil(proposal.accuracy)} metres.`:"";
  const timezone=zoneSupported?" Check the timezone.":" Choose a timezone supported by this device.";
  $("location-review").textContent=`${source} suggested.${accuracy}${timezone} Preview the timetable before saving.`;
  $("location-review").hidden=false;
  message("Location suggested. Review it and preview the timetable before saving.");
}
function applyPendingLocation() {
  if(!editing || !timezonesReady || !proposedLocation)return;
  const proposal=proposedLocation;proposedLocation=null;
  applyLocationProposal(proposal);
}
addEventListener("hashchange",()=>{
  proposedLocation=incomingLocation();
  applyPendingLocation();
});
function timeMessage(text,error=false) { $("time-format-message").textContent=text;$("time-format-message").classList.toggle("error",error); }
function fillTimeFormat(state) {
  timeEditing=structuredClone(state);timeDirty=false;timeUncertain=false;
  $("time-format").value=String(state.hours);
}
function renderTimeFormat(state) {
  timeSnapshot=state;$("time-format-card").hidden=!state;
  if(!state)return;
  if(!timeEditing || (!timeDirty && !timeUncertain))fillTimeFormat(state);
  if(state.application!=="applied")timeMessage("Time format storage is unavailable. Restart the device and reload; prayer scheduling continues.",true);
  else if(timeUncertain)timeMessage("The save response was lost. Reload time format before trying again.",true);
  else if(timeDirty && state.revision!==timeEditing.revision)timeMessage("Time format changed on another client. Your edits are preserved; reload before saving.",true);
  else if(!timeDirty)timeMessage("Time format loaded.");
}
function localTime(value,includeDate=false) {
  if(typeof value!=="string")return "Unavailable";
  const match=/^(\d{4}-\d{2}-\d{2}) ([0-2]\d):([0-5]\d)$/.exec(value);
  if(!match || Number(match[2])>23)return value;
  const hour=Number(match[2]);
  const time=timeSnapshot?.hours===12?`${hour%12 || 12}:${match[3]} ${hour<12?"AM":"PM"}`:`${match[2]}:${match[3]}`;
  return includeDate?`${match[1]} ${time}`:time;
}
function fillLights(state) {
  lightEditing=structuredClone(state);lightDirty=false;lightUncertain=false;
  if(state.settings) {
    $("lights-enabled").checked=state.settings.enabled;
    $("lights-brightness").value=state.settings.brightness_percent;
    $("lights-brightness-value").value=`${state.settings.brightness_percent}%`;
  }
}
function renderLights(state) {
  lightSnapshot=state;$("lights-card").hidden=!state?.supported;
  if(!state?.supported)return;
  if(!lightEditing || (!lightDirty && !lightUncertain))fillLights(state);
  if(state.application==="storage_fault")lightMessage("Light settings could not be read. Lights are off. Restart the device; existing data has been retained.",true);
  else if(state.application==="save_failed")lightMessage("Light settings could not be saved. The last confirmed settings remain active until restart. Restart the device and reload light settings.",true);
  else if(state.application==="output_unavailable")lightMessage("The lights are unavailable. The device will retry; prayer scheduling continues.",true);
  else if(lightUncertain)lightMessage("The save response was lost. Reload light settings before trying again.",true);
  else if(lightDirty && state.revision!==lightEditing.revision)lightMessage("Light settings changed on another client. Your edits are preserved; reload before saving.",true);
  else if(!lightDirty)lightMessage("Light settings loaded.");
}
function screenMessage(text,error=false) { $("screen-message").textContent=text;$("screen-message").classList.toggle("error",error); }
function fillScreen(state) {
  screenEditing=structuredClone(state);screenDirty=false;screenUncertain=false;
  $("screen-brightness").value=state.brightness_percent;
  $("screen-brightness-value").value=`${state.brightness_percent}%`;
}
function renderScreen(state) {
  screenSnapshot=state;$("screen-card").hidden=!state?.supported;
  if(!state?.supported)return;
  if(!screenEditing || (!screenDirty && !screenUncertain))fillScreen(state);
  if(state.application==="storage_fault")screenMessage("Screen brightness could not be read. Using 10%. Restart the device; existing data has been retained.",true);
  else if(state.application==="save_failed")screenMessage("Screen brightness could not be saved. Restart the device and reload screen brightness.",true);
  else if(state.application==="output_unavailable")screenMessage("Screen brightness is saved, but the backlight is unavailable. Restart the device if it does not recover; prayer scheduling continues.",true);
  else if(screenUncertain)screenMessage("The save response was lost. Reload screen brightness before trying again.",true);
  else if(screenDirty && state.revision!==screenEditing.revision)screenMessage("Screen brightness changed on another client. Your edits are preserved; reload before saving.",true);
  else if(!screenDirty)screenMessage("Screen brightness loaded.");
}
function controls() {
  const screenBlocked=busy || loading || !screenSnapshot?.supported || !screenEditing || ["storage_fault","save_failed"].includes(screenSnapshot.application);
  $("screen-brightness").disabled=screenBlocked;
  $("screen-save").disabled=screenBlocked || screenUncertain;
  $("screen-reload").disabled=busy || loading || !screenSnapshot?.supported;
  const timeBlocked=busy || loading || !timeEditing || timeSnapshot?.application!=="applied";
  $("time-format").disabled=timeBlocked;
  $("time-format-save").disabled=timeBlocked || timeUncertain;
  $("time-format-reload").disabled=busy || loading || !timeSnapshot;
  const lightsBlocked=busy || loading || !lightEditing?.settings || ["storage_fault","save_failed"].includes(lightSnapshot?.application);
  $("lights-fields").disabled=lightsBlocked;
  $("lights-brightness").disabled=!$("lights-enabled").checked;
  $("lights-save").disabled=lightsBlocked || lightUncertain;
  $("lights-reload").disabled=busy || loading || !lightSnapshot?.supported;
  const unavailable=busy || !editing || snapshot?.application==="storage_fault" || snapshot?.setup==="storage_fault";
  $("fields").disabled=unavailable;
  $("save").disabled=unavailable || locationNeedsPreview;
  $("preview").disabled=unavailable;
  $("reload").disabled=busy || loading || !snapshot;
  $("stop").disabled=busy || !snapshot?.playing;
  $("skip").disabled=busy || snapshot?.setup!=="active" || !snapshot?.next || !!snapshot?.skip;
  $("cancel-skip").disabled=busy || !snapshot?.skip;
  $("cancel-skip").hidden=!snapshot?.skip;
  $("save").textContent=snapshot?.setup==="active"?"Save settings":"Finish setup";
}
async function request(path, body) {
  const response=await fetch(path,{method:body===undefined?"GET":"POST",credentials:"same-origin",cache:"no-store",headers:body===undefined?{}:{"Content-Type":"application/json"},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(12000)});
  const data=await response.json();
  if(!response.ok) {const error=new Error(data.error || "Device request failed");error.status=response.status;throw error;}
  return data;
}
function times(id, schedule) {
  if(id==="preview-times")previewSchedule=schedule;
  const target=$(id);target.replaceChildren();
  for(const row of schedule?.times || []) {
    const name=document.createElement("dt"),value=document.createElement("dd");
    name.textContent=row.name;value.textContent=localTime(row.local);
    target.append(name,value);
  }
}
function render(state) {
  $("test-banner").hidden=state.test_mode!==true;
  snapshot=state;
  renderTimeFormat(state.time_format);
  renderLights(state.lights);
  renderScreen(state.display);
  renderFirmware(state.firmware);
  $("setup-state").textContent=state.setup==="active"?"Setup complete":state.setup==="incomplete"?"Setup incomplete":"Storage fault";
  $("next").textContent=state.setup!=="active"?"Finish setup to enable announcements":state.next?`${state.next.name} · ${localTime(state.next.local,true)}`:"No upcoming announcement available";
  const status=[];
  if(!state.clock_ready)status.push("Waiting for time synchronization");
  if(state.application==="volume_pending")status.push("Settings saved; applying volume");
  if(state.application==="volume_failed")status.push("Settings saved; volume could not be applied. Automatic playback is paused");
  if(state.application==="storage_fault" || state.setup==="storage_fault")status.push("Storage unavailable. Restart the device; existing data has been retained");
  if(state.scheduler_fault && state.scheduler_fault!=="none")status.push(faults[state.scheduler_fault] || "Device fault. Restart and check its status");
  if(state.skip)status.push("The selected announcement will be skipped");
  if(!state.wifi_connected)status.push("Wi-Fi disconnected");
  $("health").textContent=status.join(" · ") || (state.automatic_ready?"Ready for the next announcement":"Waiting for setup or device readiness");
  $("address").textContent=state.hostname;
  times("times",state.schedule);
  if(previewSchedule)times("preview-times",previewSchedule);
  $("conflicts").textContent=(state.schedule?.conflicts || []).join(" ");
  $("setup-help").hidden=state.setup==="active";
  controls();
}
function fill(state) {
  editing=structuredClone(state);
  const s=state.settings;if(!s){editing=undefined;controls();return;}
  locationProposal=undefined;locationNeedsPreview=false;$("location-review").hidden=true;
  const fresh=state.setup==="incomplete" && state.revision===1;
  $("latitude").value=fresh?"":s.latitude;$("longitude").value=fresh?"":s.longitude;
  $("timezone").value=fresh?"":s.timezone;$("method").value=fresh?"":s.method;
  $("asr").value=s.asr_method;$("high-latitude").value=s.high_latitude;
  $("volume").value=s.volume;$("volume-value").value=`${s.volume}%`;
  for(const name of prayers)$("enabled-"+name).checked=s.enabled[name];
  for(const name of events)$("offset-"+name).value=s.offsets[name];
  $("refresh-timezone").checked=false;$("preview-section").hidden=true;dirty=false;controls();
}
function documentFromForm() {
  if(!$("settings").reportValidity())return null;
  const s=structuredClone(editing.settings);
  s.latitude=Number($("latitude").value);s.longitude=Number($("longitude").value);
  s.timezone=$("timezone").value.trim();s.method=$("method").value;
  s.asr_method=$("asr").value;s.high_latitude=$("high-latitude").value;s.volume=Number($("volume").value);
  for(const name of prayers)s.enabled[name]=$("enabled-"+name).checked;
  for(const name of events)s.offsets[name]=Number($("offset-"+name).value);
  return {schema:1,expected_revision:editing.revision,settings:s,refresh_timezone:$("refresh-timezone").checked};
}
async function refresh(replace=false) {
  if(loading || busy)return false;
  loading=true;controls();
  const started=generation;
  try {
    const state=await request("/api/status");if(started!==generation)return false;render(state);
    if(replace || !editing)fill(state);
    else if(state.revision!==editing.revision && !dirty)fill(state);
    else if(state.revision!==editing.revision)message("Settings changed on another client. Your edits are preserved; reload the saved settings before saving.",true);
    applyPendingLocation();
    return true;
  }catch(error){message(error.message+". Check the device connection.",true);return false;}
  finally{loading=false;controls();}
}
async function action(path,body) {
  if(busy)return;
  busy=true;++generation;controls();
  try { const state=await request(path,body);render(state);if(path==="/api/settings" || path==="/api/activate")fill(state);message(state.setup==="incomplete"?"Settings saved. Setup is still incomplete.":"Device updated."); }
  catch(error) {
    message(error.message,true);
    // A response may be lost after commit. Read once; never repeat the mutation.
    try {
      const state=await request("/api/status");render(state);
      if(!error.status){
        message("The response was lost. Current device status has been read back. Review it and reload saved settings before trying again.",true);
      }
    }catch{message("Connection lost. Reconnect and reload saved settings before trying again.",true);}
  }finally{busy=false;controls();}
}
for(const [value,label] of Object.entries(methods))$("method").add(new Option(label,value));
for(const name of prayers) {
  const label=document.createElement("label"),input=document.createElement("input");
  label.className="check";input.type="checkbox";input.id="enabled-"+name;label.append(input,document.createTextNode(title(name)));$("enabled").append(label);
}
for(const name of events) {
  const label=document.createElement("label"),input=document.createElement("input");
  input.type="number";input.id="offset-"+name;input.min=-120;input.max=120;input.step=1;input.required=true;
  label.append(document.createTextNode(title(name)),input);$("offsets").append(label);
}
$("settings").addEventListener("input",()=>{++settingsVersion;dirty=true;if(locationProposal)locationNeedsPreview=true;$("volume-value").value=`${$("volume").value}%`;$("preview-section").hidden=true;controls();});
$("settings").addEventListener("submit",(event)=>{event.preventDefault();if(locationNeedsPreview){message("Preview the timetable before saving this location.",true);return;}const body=documentFromForm();if(body)action(snapshot.setup==="active"?"/api/settings":"/api/activate",body);});
$("preview").addEventListener("click",async()=>{
  const body=documentFromForm();if(!body || busy)return;const version=settingsVersion;busy=true;controls();
  try {const data=await request("/api/preview",body);if(version!==settingsVersion)return;times("preview-times",data);$("preview-message").textContent=data.state==="waiting_for_time"?locationProposal?"Waiting for time synchronization. Preview the timetable after the clock is ready before saving this location.":"Waiting for time synchronization. You can finish setup now; announcements will wait for a valid clock.":data.state==="invalid_schedule"?"These settings do not produce a valid schedule. Review the location, method and offsets.":(data.conflicts || []).join(" ");$("preview-section").hidden=false;if(locationProposal && data.state==="ready"){locationNeedsPreview=false;controls();}}
  catch(error){message(error.message,true);}finally{busy=false;controls();}
});
$("refresh").addEventListener("click",()=>refreshWithTimezones());
$("reload").addEventListener("click",async()=>{if(dirty && !confirm("Replace your unsaved edits with the device's saved settings?"))return;if(await refresh(true))message("Saved settings loaded.");});
$("stop").addEventListener("click",()=>action("/api/stop",{}));
$("skip").addEventListener("click",()=>action("/api/skip",{expected_revision:snapshot.revision,occurrence:snapshot.next}));
$("cancel-skip").addEventListener("click",()=>action("/api/cancel-skip",{expected_revision:snapshot.revision,occurrence:snapshot.skip}));
$("lights-form").addEventListener("input",()=>{
  lightDirty=true;$("lights-brightness-value").value=`${$("lights-brightness").value}%`;controls();
});
$("lights-reload").addEventListener("click",async()=>{
  if(busy || loading || (lightDirty && !confirm("Replace your unsaved light edits with the saved settings?")))return;
  busy=true;++generation;controls();
  try {const state=await request("/api/lights");fillLights(state);renderLights(state);}
  catch(error){lightMessage(error.message+". Check the device connection.",true);}
  finally{busy=false;controls();}
});
$("lights-form").addEventListener("submit",async(event)=>{
  event.preventDefault();if(busy || loading || lightUncertain || !lightEditing?.settings)return;
  const settings={enabled:$("lights-enabled").checked,brightness_percent:Number($("lights-brightness").value)};
  const revision=lightEditing.revision;
  busy=true;++generation;controls();
  try {
    const state=await request("/api/lights",{schema:1,expected_revision:revision,settings});
    fillLights(state);renderLights(state);
    if(state.application==="applied")lightMessage("Light settings saved.");
  } catch(error) {
    lightUncertain=!error.status;lightMessage(error.message,true);
    // Reconcile a lost acknowledgement once; never repeat the write automatically.
    try {
      const state=await request("/api/lights");
      const matches=state.settings?.enabled===settings.enabled && state.settings?.brightness_percent===settings.brightness_percent;
      // Chromium may retry a transport-failed POST; its stale revision is rejected.
      // A matching committed revision also reconciles that resulting 409 safely.
      if((!error.status || error.status===409) && matches &&
          (state.revision===revision+1 || (!error.status && state.revision===revision))) {
        fillLights(state);renderLights(state);
        if(state.application==="applied")lightMessage("Saved light settings confirmed after reconnecting.");
      } else {
        renderLights(state);
        if(error.status===409)lightMessage("Light settings changed on another client. Your edits are preserved; reload before saving.",true);
      }
    } catch {lightUncertain=true;lightMessage("Connection lost. Reload light settings before trying again.",true);}
  } finally {busy=false;controls();}
});
$("screen-brightness").addEventListener("input",()=>{
  screenDirty=true;$("screen-brightness-value").value=`${$("screen-brightness").value}%`;controls();
});
$("screen-reload").addEventListener("click",async()=>{
  if(busy || loading || (screenDirty && !confirm("Replace your unsaved screen brightness with the saved setting?")))return;
  busy=true;++generation;controls();
  try {const state=await request("/api/display");fillScreen(state);renderScreen(state);}
  catch(error){screenMessage(error.message+". Check the device connection.",true);}
  finally{busy=false;controls();}
});
$("screen-form").addEventListener("submit",async(event)=>{
  event.preventDefault();if(busy || loading || screenUncertain || !screenEditing || !screenSnapshot?.supported)return;
  const brightness_percent=Number($("screen-brightness").value),revision=screenEditing.revision;
  busy=true;++generation;controls();
  try {
    const state=await request("/api/display",{schema:1,expected_revision:revision,brightness_percent});
    fillScreen(state);renderScreen(state);
    if(state.application==="applied")screenMessage("Screen brightness saved.");
  } catch(error) {
    screenUncertain=!error.status;screenMessage(error.message,true);
    // Read back once after an uncertain save; never repeat the write automatically.
    try {
      const state=await request("/api/display");
      if((!error.status || error.status===409) && state.brightness_percent===brightness_percent &&
          (state.revision===revision+1 || (!error.status && state.revision===revision))) {
        fillScreen(state);renderScreen(state);
        if(state.application==="applied")screenMessage("Saved screen brightness confirmed after reconnecting.");
      } else {
        renderScreen(state);
        if(error.status===409)screenMessage("Screen brightness changed on another client. Your edits are preserved; reload before saving.",true);
      }
    } catch {screenUncertain=true;screenMessage("Connection lost. Reload screen brightness before trying again.",true);}
  } finally{busy=false;controls();}
});
$("time-format").addEventListener("change",()=>{timeDirty=true;timeMessage("Time format changes apply when you save.");});
function applyTimeFormat(state) { render({...snapshot,time_format:state}); }
$("time-format-reload").addEventListener("click",async()=>{
  if(busy || loading || (timeDirty && !confirm("Replace your unsaved time format with the saved setting?")))return;
  busy=true;++generation;controls();
  try {const state=await request("/api/time-format");fillTimeFormat(state);applyTimeFormat(state);}
  catch(error){timeMessage(error.message+". Check the device connection.",true);}
  finally{busy=false;controls();}
});
$("time-format-form").addEventListener("submit",async(event)=>{
  event.preventDefault();if(busy || loading || timeUncertain || !timeEditing)return;
  const hours=Number($("time-format").value),revision=timeEditing.revision;
  busy=true;++generation;controls();
  try {
    const state=await request("/api/time-format",{schema:1,expected_revision:revision,hours});
    fillTimeFormat(state);applyTimeFormat(state);timeMessage("Time format saved.");
  } catch(error) {
    timeUncertain=!error.status;timeMessage(error.message,true);
    try {
      const state=await request("/api/time-format");
      if((!error.status || error.status===409) && state.hours===hours &&
          (state.revision===revision+1 || (!error.status && state.revision===revision))) {
        fillTimeFormat(state);applyTimeFormat(state);
        if(state.application==="applied")timeMessage("Saved time format confirmed after reconnecting.");
      } else applyTimeFormat(state);
    } catch {timeUncertain=true;timeMessage("Connection lost. Reload time format before trying again.",true);}
  } finally{busy=false;controls();}
});
async function loadTimezones() {
  if(timezonesReady || timezonesLoading)return;
  timezonesLoading=true;
  try {
    const data=await request("/api/timezones");
    if(!Array.isArray(data.names) || !data.names.length || data.names.some(name=>typeof name!=="string"))throw new Error("Invalid timezone list");
    for(const name of data.names){supportedZones.add(name);$("zones").append(new Option(name,name));}
    timezonesReady=true;
    if(editing)message("Connected to your OpenAthan.");
    applyPendingLocation();
  }catch(error){message(proposedLocation && !proposedLocation.error?"Timezone list unavailable. Your location suggestion is waiting. Select Refresh to retry.":"Timezone list unavailable. Select Refresh to retry.",true);}
  finally{timezonesLoading=false;}
}
async function refreshWithTimezones() {
  await refresh();
  if(!timezonesReady)await loadTimezones();
}
async function start() {
  await refreshWithTimezones();
  setInterval(()=>{if(!document.hidden)refreshWithTimezones();},5000);
}
let firmwareBusy=false, firmwareUncertain=false, firmwareGeneration=0, firmwareState;
let firmwareExpected="";
try {firmwareExpected=sessionStorage.getItem("firmware-expected") || "";}catch {}
function rememberFirmware(version="") {
  firmwareExpected=version;
  // This is only a reconnect hint. Device status remains authoritative when
  // browser privacy policy or a storage quota prevents saving it.
  try {
    if(version)sessionStorage.setItem("firmware-expected",version);
    else sessionStorage.removeItem("firmware-expected");
  }catch {}
}
const firmwareMessages={idle:"No update has been requested.",checking:"Checking for a stable release…",current:"Your firmware is up to date.",available:"A firmware update is available.",queued:"Update queued. Waiting for a safe time between prayers.",downloading:"Downloading the update…",verifying:"Verifying the downloaded firmware…",restarting:"Restarting with the new firmware. Keep the speaker powered.",success:"Firmware updated successfully.",rolled_back:"The update could not start successfully. The previous firmware has been restored.",failed:"The update could not complete. Check again to retry.",storage_fault:"Update storage is unavailable. Restart the device; saved data has been retained."};
function renderFirmware(state,confirmed=false) {
  if(confirmed)firmwareUncertain=false;
  firmwareState=state;$("firmware-section").hidden=!state;
  if(!state)return;
  if(firmwareExpected && state.version===firmwareExpected && state.result==="success") {
    rememberFirmware();
  } else if(["rolled_back","superseded"].includes(state.result)) {
    rememberFirmware();
  }
  $("firmware-version").textContent=`Installed: ${state.version}${state.queued_version?` · Queued: ${state.queued_version}`:state.available?` · Available: ${state.available.version}`:""}`;
  const progress=state.state==="downloading" && state.total?` ${Math.floor(100*state.received/state.total)}%`:"";
  $("firmware-status").textContent=(state.supported===false?"This speaker needs a maintainer USB update before Wi-Fi installation. ":"")+(firmwareMessages[state.state] || "Reading update status…")+progress+(state.error?` ${state.error}`:"");
  if(firmwareUncertain)$("firmware-status").textContent="The response was lost. Waiting to read update status before allowing another action. Keep the speaker powered.";
  $("firmware-last-check").textContent=state.last_check?`Last checked: ${new Date(state.last_check*1000).toLocaleString(undefined,{hour12:timeSnapshot?.hours===12})}`:"No successful update check yet.";
  const working=["checking","queued","downloading","verifying","restarting"].includes(state.state);
  $("firmware-check").disabled=firmwareBusy || firmwareUncertain || working || state.state==="storage_fault";
  $("firmware-install").hidden=!state.available || working;
  $("firmware-install").disabled=firmwareBusy || firmwareUncertain || state.state==="storage_fault" || state.supported===false;
  $("firmware-cancel").hidden=!["queued","downloading","verifying"].includes(state.state);
  $("firmware-cancel").disabled=firmwareBusy || firmwareUncertain;
  $("firmware-notes").hidden=!state.available;
  if(state.available)$("firmware-notes").href=`https://github.com/OpenAthan-Project/openathan/releases/tag/${encodeURIComponent(state.available.version)}`;
}
async function firmwareAction(action) {
  if(firmwareBusy || firmwareUncertain || !firmwareState)return;
  ++generation;++firmwareGeneration;
  firmwareBusy=true;renderFirmware(firmwareState);
  const version=firmwareState.available?.version;
  try {
    if(action==="install")rememberFirmware(version);
    renderFirmware(await request(`/api/firmware/${action}`,action==="install"?{version}:{}),true);
    if(action==="cancel")rememberFirmware();
  } catch(error) {
    firmwareUncertain=!error.status;
    $("firmware-status").textContent=`${error.message}. Reading update status before retrying.`;
    try {renderFirmware(await request("/api/firmware"),true);}catch {$("firmware-status").textContent="The speaker is unavailable. Keep it powered; status will reconnect automatically.";}
  } finally {firmwareBusy=false;if(firmwareState)renderFirmware(firmwareState);}
}
for(const action of ["check","install","cancel"])$("firmware-"+action).addEventListener("click",()=>firmwareAction(action));
setInterval(async()=>{
  if(firmwareBusy || !firmwareState)return;
  const started=firmwareGeneration;
  try {const state=await request("/api/firmware");if(started===firmwareGeneration && !firmwareBusy)renderFirmware(state,true);}
  catch {if(firmwareExpected || ["downloading","verifying","restarting"].includes(firmwareState.state))$("firmware-status").textContent="Waiting for the speaker to reconnect. Keep it powered.";}
},3000);
start();

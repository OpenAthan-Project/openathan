"use strict";
const $ = (id) => document.getElementById(id);
const prayers = ["fajr", "dhuhr", "asr", "maghrib", "isha"];
const events = ["fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"];
const methods = {muslim_world_league:"Muslim World League",egyptian:"Egyptian",karachi:"Karachi",umm_al_qura:"Umm al-Qura",dubai:"Dubai",moonsighting_committee:"Moonsighting Committee",north_america:"North America (ISNA)",kuwait:"Kuwait",qatar:"Qatar",singapore:"Singapore",tehran:"Tehran",turkey:"Turkey"};
const faults = {"invalid settings":"Review the location, timezone and calculation settings, then save again","invalid prayer schedule":"These settings cannot produce a valid schedule. Review the location, method and offsets","durable state unavailable":"Saved data could not be read or written. Restart the device; do not erase its storage","audio unavailable":"Audio is unavailable. Check the device power and compatible audio installation","playback request rejected":"Audio could not start. Check the device power and audio installation"};
let snapshot, editing, dirty = false, busy = false, loading = false, generation = 0;
const title = (s) => s[0].toUpperCase()+s.slice(1);
function message(text, error=false) { $("message").textContent=text; $("message").classList.toggle("error",error); }
function controls() {
  const unavailable=busy || !editing || snapshot?.application==="storage_fault" || snapshot?.setup==="storage_fault";
  $("fields").disabled=unavailable;
  for(const id of ["save","preview"]) $(id).disabled=unavailable;
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
  const target=$(id);target.replaceChildren();
  for(const row of schedule?.times || []) {
    const name=document.createElement("dt"),value=document.createElement("dd");
    name.textContent=row.name;value.textContent=row.local?.slice(11) || row.local || "Unavailable";
    target.append(name,value);
  }
}
function render(state) {
  snapshot=state;
  $("setup-state").textContent=state.setup==="active"?"Setup complete":state.setup==="incomplete"?"Setup incomplete":"Storage fault";
  $("next").textContent=state.setup!=="active"?"Finish setup to enable announcements":state.next?`${state.next.name} · ${state.next.local}`:"No upcoming announcement available";
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
  $("conflicts").textContent=(state.schedule?.conflicts || []).join(" ");
  $("setup-help").hidden=state.setup==="active";
  controls();
}
function fill(state) {
  editing=structuredClone(state);
  const s=state.settings;if(!s){editing=undefined;controls();return;}
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
$("settings").addEventListener("input",()=>{dirty=true;$("volume-value").value=`${$("volume").value}%`;$("preview-section").hidden=true;});
$("settings").addEventListener("submit",(event)=>{event.preventDefault();const body=documentFromForm();if(body)action(snapshot.setup==="active"?"/api/settings":"/api/activate",body);});
$("preview").addEventListener("click",async()=>{
  const body=documentFromForm();if(!body || busy)return;busy=true;controls();
  try {const data=await request("/api/preview",body);times("preview-times",data);$("preview-message").textContent=data.state==="waiting_for_time"?"Waiting for time synchronization. You can finish setup now; announcements will wait for a valid clock.":data.state==="invalid_schedule"?"These settings do not produce a valid schedule. Review the location, method and offsets.":(data.conflicts || []).join(" ");$("preview-section").hidden=false;}
  catch(error){message(error.message,true);}finally{busy=false;controls();}
});
$("refresh").addEventListener("click",()=>refresh());
$("reload").addEventListener("click",async()=>{if(dirty && !confirm("Replace your unsaved edits with the device's saved settings?"))return;if(await refresh(true))message("Saved settings loaded.");});
$("stop").addEventListener("click",()=>action("/api/stop",{}));
$("skip").addEventListener("click",()=>action("/api/skip",{expected_revision:snapshot.revision,occurrence:snapshot.next}));
$("cancel-skip").addEventListener("click",()=>action("/api/cancel-skip",{expected_revision:snapshot.revision,occurrence:snapshot.skip}));
async function start() {
  await refresh();
  try {const data=await request("/api/timezones");for(const name of data.names)$("zones").append(new Option(name,name));if(editing)message("Connected to your OpenAthan.");}
  catch(error){message("Timezone list unavailable. Refresh the page to retry.",true);}
  setInterval(()=>{if(!document.hidden)refresh();},5000);
}
start();

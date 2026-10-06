"use strict";
const $=id=>document.getElementById(id),clone=v=>structuredClone(v);
const prayers=["fajr","dhuhr","asr","maghrib","isha"],events=["fajr","sunrise","dhuhr","asr","maghrib","isha"];
const title=s=>s[0].toUpperCase()+s.slice(1);
const domains={};
const responseOrders=new WeakMap();let requestOrder=0,statusOrder=0,actionOrder=0;
const specs={settings:{path:"/api/settings",read:"/api/status",groups:["volume","preferences"]},display:{path:"/api/display",read:"/api/display",groups:["screen"]},lights:{path:"/api/lights",read:"/api/lights",groups:["lights"]},time_format:{path:"/api/time-format",read:"/api/time-format",groups:["format"]}};
let snapshot,timeSnapshot,connected=false,loading=false,attempted=false,writeBusy=false,generation=0;
let draft,draftDirty=false,draftVersion=0,preview,helperDraft=false,firstRun=false,setupStep=0;
let actionBusy=false,actionUncertain=false,pendingAction,stopBusy=false,stopAwaiting=false,playbackVersion=0;
let timezonesReady=false,timezonesLoading=false,proposedLocation;
const supportedZones=new Set(),sliderTimers=new Map(),renderKeys=new Map();
function merge(base,patch){const out=clone(base);for(const [k,v] of Object.entries(patch))out[k]=v&&typeof v==="object"&&!Array.isArray(v)?merge(out[k]||{},v):v;return out;}
function stable(v){return JSON.stringify(v,(_,value)=>value&&typeof value==="object"&&!Array.isArray(value)?Object.fromEntries(Object.keys(value).sort().map(k=>[k,value[k]])):value);}
function text(id,value){if($(id).textContent!==value)$(id).textContent=value;}
function status(group,value,type=""){text(group+"-feedback",value);$(group+"-feedback").className="feedback"+(type?" "+type:"");}
function notify(key,value,type="",groups=specs[key].groups){groups.forEach(g=>status(g,value,type));}
function current(key){const d=domains[key];return d?.snapshot.value?merge(d.snapshot.value,d.desired):null;}
function valueFrom(key,state){return key==="settings"||key==="lights"?state.settings:key==="display"?{brightness_percent:state.brightness_percent}:{hours:state.hours};}
function domainState(key,state){return {revision:state.revision,value:clone(valueFrom(key,state)),application:state.application};}
function sameKey(a,b){return !!a&&!!b&&a.day===b.day&&a.prayer===b.prayer;}
function isSkipped(next,skip){return sameKey(next,skip)||sameKey(next?.shared_with,skip);}
function dateLabel(value){if(!/^\d{4}-\d{2}-\d{2}$/.test(value||""))return "";return new Intl.DateTimeFormat(undefined,{weekday:"long",day:"numeric",month:"long",timeZone:"UTC"}).format(new Date(value+"T12:00:00Z"));}
function localTime(value){const m=/^(\d{4}-\d{2}-\d{2}) ([0-2]\d):([0-5]\d)$/.exec(value||"");if(!m||+m[2]>23)return "Unavailable";return timeSnapshot?.hours===12?(+m[2]%12||12)+":"+m[3]+(+m[2]<12?" AM":" PM"):m[2]+":"+m[3];}
async function request(path,body){
 const order=++requestOrder;
 if(["/api/skip","/api/cancel-skip"].includes(path))actionOrder=order;
 const response=await fetch(path,{method:body===undefined?"GET":"POST",credentials:"same-origin",cache:"no-store",headers:body===undefined?{}:{"Content-Type":"application/json"},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(12000)});
 let data;try{data=await response.json();}catch{throw new Error("The response could not be confirmed");}
 if(!response.ok){const error=new Error(data.error||"Device request failed");error.status=response.status;throw error;}
 if(data&&typeof data==="object"){responseOrders.set(data,order);for(const key of ["display","lights","time_format","firmware"])if(data[key]&&typeof data[key]==="object")responseOrders.set(data[key],order);}return data;
}
function calculation(value){const out=clone(value);delete out.volume;delete out.enabled;return out;}
function settingsFault(state=snapshot){return state?.setup==="storage_fault"||["storage_fault","save_failed"].includes(state?.application);}
function preservePlayback(state,started){if(started===playbackVersion||!snapshot)return state;const next={...state,playing:snapshot.playing};responseOrders.set(next,responseOrders.get(state)||statusOrder);return next;}
function preferencePatch(patch){return Object.fromEntries(Object.entries(patch).filter(([key])=>["volume","enabled"].includes(key)));}
function fillDraft(value,fresh=false){
 draft=calculation(value);if(fresh){draft.latitude="";draft.longitude="";draft.timezone="";draft.method="";}
 for(const [id,key] of Object.entries({latitude:"latitude",longitude:"longitude",timezone:"timezone",method:"method",asr:"asr_method","high-latitude":"high_latitude"}))$(id).value=draft[key];
 for(const p of events)$("offset-"+p).value=draft.offsets[p];
 $("refresh-timezone").checked=false;
}
function readDraft(){for(const [id,key] of Object.entries({latitude:"latitude",longitude:"longitude",timezone:"timezone",method:"method",asr:"asr_method","high-latitude":"high_latitude"}))draft[key]=["latitude","longitude"].includes(id)?$(id).value===""?"":Number($(id).value):$(id).value.trim();draft.offsets=Object.fromEntries(events.map(p=>[p,Number($("offset-"+p).value)]));}
function markDraft(){readDraft();draftDirty=true;++draftVersion;preview=undefined;status("prayer","Draft · preview before saving","warning");renderDraftControls();}
function acceptDomain(key,state,ack=false){
 if(!state||!Number.isInteger(state.revision))return;
 const fault=key!=="settings"&&settingsFault(state);let d=domains[key];
 if(!fault&&!valueFrom(key,state))return;
 const next=fault?{revision:d?.snapshot.revision??state.revision,value:d?.snapshot.value??(state.revision>0&&valueFrom(key,state)?clone(valueFrom(key,state)):null),application:state.application}:domainState(key,state);
 if(!d){d=domains[key]={snapshot:next,desired:{},pending:{},busy:false,blocked:null,groups:new Set(),meta:null};return;}
 if(next.revision<d.snapshot.revision&&!settingsFault(d.snapshot)||d.busy&&!ack&&!fault)return;
 const changed=next.revision!==d.snapshot.revision;
 if(changed&&key==="settings"&&stable(calculation(next.value))!==stable(calculation(d.snapshot.value))){
  preview=undefined;if(draftDirty)status("prayer","Saved prayer settings changed. Your draft is kept; preview again.","warning");
  else if(!firstRun)fillDraft(next.value);
 }
 if(changed&&!ack&&Object.keys(d.desired).length){d.blocked="conflict";notify(key,"Changed on another client. Your edits are kept; choose which values to use.","error");recovery(key);}
 d.snapshot=next;
}
function acceptStatus(state,ack,order=responseOrders.get(state)||statusOrder){
 if(!Number.isInteger(state?.revision)||!state.settings&&!settingsFault(state))throw new Error("Device status is incomplete");
 if(snapshot?.settings&&state.settings&&state.revision<snapshot.revision)return;
 for(const key of ["display","lights","time_format"]){
  const known=snapshot?.[key],next=state[key],older=(responseOrders.get(next)||order)<(responseOrders.get(known)||0);
  if(known&&next&&(next.revision<known.revision&&!settingsFault(next)||older&&(next.revision===known.revision||settingsFault(next)||settingsFault(known))))state={...state,[key]:known};
 }
 if(state.firmware&&firmwareState&&(responseOrders.get(state.firmware)||order)<(responseOrders.get(firmwareState)||0))state={...state,firmware:firmwareState};
 statusOrder=Math.max(statusOrder,order);
 if(stopAwaiting&&state.playing===false)stoppedFeedback();
 const initial=!snapshot,recovered=settingsFault()&&!settingsFault(state);connected=true;if(actionUncertain&&order>=actionOrder){actionUncertain=false;status("action","Skip state checked · review the current prayer");queueMicrotask(pump);}snapshot=state;if(!settingsFault(state.time_format))timeSnapshot=state.time_format;
 for(const key of Object.keys(specs))acceptDomain(key,key==="settings"?state:state[key],ack===key);
 firstRun=state.setup==="incomplete";
 if(initial){if(state.settings)fillDraft(state.settings,firstRun&&state.revision===1);status("prayer",firstRun?"Setup not finished · choose your location":"Saved prayer settings",firstRun?"":"success");showView(firstRun?"settings":"today",false);}
 if(state.settings&&!draft)fillDraft(state.settings);
 if(recovered)status("prayer",draftDirty?"Saved storage recovered. Your draft is kept; preview before saving.":"Saved prayer settings",draftDirty?"warning":"success");
 if(settingsFault(state)){preview=undefined;status("prayer","Saved prayer storage is unavailable. Your draft is kept; restart and check saved state.","error");}
 if(state.firmware)renderFirmware(state.firmware);
 render();applyPendingLocation();
}
function acceptSavedState(key,raw){
 if(key==="settings")acceptStatus(raw,key);
 else{
  const known=snapshot?.[key];if(settingsFault(known)&&(responseOrders.get(raw)||0)<(responseOrders.get(known)||0))throw new Error("A newer storage fault requires saved-state readback");
  acceptDomain(key,raw,true);if(snapshot)snapshot[key]=raw;if(key==="time_format"&&!settingsFault(raw))timeSnapshot=raw;
 }
}
function savedMessage(key){
 const app=domains[key].snapshot.application;
 if(app==="storage_fault"||app==="save_failed")return ["Saved storage is unavailable. Your edits are kept; restart and check saved state.","error"];
 if(app==="output_unavailable")return [key==="display"?"Saved · backlight unavailable. The device will retry.":"Saved · lights unavailable. Scheduling continues.","warning"];
 if(app==="volume_pending")return ["Saved · applying volume…","warning"];
 if(app==="volume_failed")return ["Saved · volume could not be applied. Automatic playback is paused.","error"];
 return ["Saved to speaker","success"];
}
function renderPreferences(){
 for(const [key,ids] of Object.entries({settings:["volume","settings-volume"],display:["screen-brightness"],lights:["lights-brightness"],time_format:[]})){
  const value=current(key);if(!value){for(const id of ids){$(id).disabled=true;$(id+"-value").value="—";}continue;}
  for(const id of ids){const v=key==="settings"?value.volume:value.brightness_percent;if(document.activeElement!==$(id))$(id).value=v;$(id+"-value").value=v+"%";}
 }
 const s=current("settings");if(s)prayers.forEach(p=>$("enabled-"+p).checked=s.enabled[p]);
 $("lights-enabled").indeterminate=!current("lights");
 if(current("lights"))$("lights-enabled").checked=current("lights").enabled;
 else{$("lights-enabled").disabled=true;if(snapshot?.lights?.supported&&!domains.lights)status("lights","Saved light preferences are unavailable. Restart the speaker and check saved state.","error");}
 $("time-format").value=current("time_format")?.hours??"";
 for(const [key,d] of Object.entries(domains)){
  const fault=["storage_fault","save_failed"].includes(d.snapshot.application)||key==="settings"&&settingsFault();
  if(fault&&key!=="settings"||!d.busy&&!d.blocked&&!Object.keys(d.desired).length&&!(key==="settings"&&settingsFault()))notify(key,...savedMessage(key));
  if(key!=="settings")for(const group of specs[key].groups)$(group+"-recovery").querySelectorAll("button").forEach(b=>b.disabled=fault);
  const elements=key==="settings"?["volume","settings-volume",...prayers.map(p=>"enabled-"+p)]:key==="display"?["screen-brightness"]:key==="lights"?["lights-enabled","lights-brightness"]:["time-format"];
  elements.forEach(id=>$(id).disabled=fault||firstRun);
 }
 if(settingsFault()){
  notify("settings","Saved prayer storage is unavailable. Your edits are kept; restart and check saved state.","error");
  for(const group of ["volume","preferences","prayer"])$(group+"-recovery").querySelectorAll("button").forEach(b=>b.disabled=true);
 }else for(const group of ["volume","preferences","prayer"])$(group+"-recovery").querySelectorAll("button").forEach(b=>b.disabled=false);
 $("hardware-group").hidden=!snapshot?.display?.supported&&!snapshot?.lights?.supported;
 $("screen-controls").hidden=!snapshot?.display?.supported;$("light-controls").hidden=!snapshot?.lights?.supported;
 $("format-group").hidden=!snapshot?.time_format;
}
function renderTimes(id,schedule,settings,isPreview=false){
 const next=snapshot?.clock_ready&&snapshot?.setup==="active"?snapshot.next:null;
 const signature=stable([schedule,settings?.enabled,isPreview?null:next,isPreview?null:snapshot?.skip,timeSnapshot?.hours,isPreview?false:settingsFault()]);
 if(renderKeys.get(id)===signature)return;renderKeys.set(id,signature);
 const rows=(schedule?.times||[]).map(item=>{
  const p=item.name?.toLowerCase(),row=document.createElement("div"),dt=document.createElement("dt"),dd=document.createElement("dd"),small=document.createElement("small");
  row.className="time-row";row.dataset.prayer=p;dt.textContent=item.name;
  if(!isPreview&&next&&prayers[next.prayer]===p&&Number.isFinite(item.utc)&&item.utc===next.utc){row.setAttribute("aria-current","time");const label=document.createElement("span");label.className="visually-hidden";label.textContent=", next prayer";dt.append(label);}
  small.textContent=p==="sunrise"?"No Athan":!settings?.enabled?.[p]?"Athan off":!isPreview&&isSkipped(next,snapshot?.skip)&&row.hasAttribute("aria-current")?"Skipped":"Athan on";
  dt.append(small);dd.textContent=localTime(item.local);
  if(isPreview){const before=snapshot?.schedule?.times?.find(t=>t.name===item.name);if(before?.local&&before.local!==item.local){const label=document.createElement("small");label.className="before-time";label.textContent="Was "+localTime(before.local);dd.append(label);}}
  row.append(dt,dd);return row;
 });
 $(id).replaceChildren(...rows);
 if(!rows.length){const p=document.createElement("p");p.className="hint";p.textContent=!isPreview&&settingsFault()?"The timetable is unavailable while saved prayer storage needs attention.":schedule?.state==="waiting_for_time"?"A timetable will appear after the speaker’s clock is ready.":schedule?.state==="invalid_schedule"?"These settings cannot produce a valid timetable. Review prayer settings.":"Finish setup to see your timetable.";$(id).append(p);}
}
function render(){
 const state=snapshot,next=state?.clock_ready&&state?.setup==="active"?state.next:null,skipped=isSkipped(next,state?.skip);
 $("connection-banner").hidden=connected||!snapshot&&!attempted;
 text("connection-heading",snapshot?"Connection lost":"Speaker unavailable");
 text("connection-detail",snapshot?"Displayed information is stale. The speaker’s current playback state is unknown.":"Device status is unavailable. Check the speaker and connection, then reconnect.");
 text("address",state?.hostname||"Connecting to speaker");const date=state?.clock_ready?dateLabel(state.local_date):"";text("device-date",date);$("device-date").hidden=!date;
 $("test-banner").hidden=state?.test_mode!==true;text("next-name",next?.name||"—");text("next-time",next?localTime(next.local):"—");
 let nextDate=$("next-date");if(!nextDate){nextDate=document.createElement("p");nextDate.id="next-date";nextDate.className="next-date";$("next-time").after(nextDate);}
 const onTable=next&&state.schedule?.times?.some(t=>Number.isFinite(t.utc)&&t.utc===next.utc&&t.name?.toLowerCase()===prayers[next.prayer]);
 text("next-date",next&&!onTable?dateLabel(next.local?.slice(0,10)):"");nextDate.hidden=!nextDate.textContent;
 const readiness=!state?"Connecting to speaker":!connected?"Connection lost · stale":state.playing?"Playing":settingsFault()?"Saved storage unavailable":actionUncertain?"Skip state unconfirmed":state.application==="volume_failed"?"Automatic playback paused":state.application==="volume_pending"?"Applying volume":!state.clock_ready?"Waiting for time":firstRun?"Finish setup to enable Athan":skipped?"Athan skipped"+(state.local_date&&next.local?.slice(0,10)!==state.local_date?" on "+next.local.slice(0,10):" today"):state.automatic_ready?"Ready to play":"Waiting for device readiness";
 text("readiness-text",readiness);$("readiness").style.color=state?.automatic_ready&&connected&&!skipped?"var(--good)":"var(--amber)";
 const detail=!connected&&state?"Last observed: "+(state.playing?"playing":"idle")+". Playback may still be active.":settingsFault()?(state.playing?"Playback is active. ":"")+"Saved prayer storage is unavailable. Restart the speaker and check saved state.":state?.playing?"Playback is active on the speaker.":state?.application==="volume_failed"?"Volume could not be applied. Check the speaker and saved volume.":state&&!state.clock_ready?"Announcements will wait until the device clock is valid.":state?.scheduler_fault&&state.scheduler_fault!=="none"?"Device needs attention: "+state.scheduler_fault+". Check the speaker and prayer settings.":"";
 text("state-detail",detail);$("state-detail").hidden=!detail;
 for(const id of ["stop","settings-stop"]){$(id).hidden=!state?.playing;$(id).disabled=!connected||stopBusy;}
 $("settings-playing").hidden=!state?.playing;$("settings-playing").querySelector(".hint").textContent=connected?"Playback is active on the speaker.":"Last observed playing · connection lost.";
 const skipKey=state?.skip,skipName=skipKey?title(prayers[skipKey.prayer]):next?.name;
 const actionDate=next&&state.local_date&&next.local?.slice(0,10)!==state.local_date?" on "+next.local.slice(0,10):" today";
 text("skip",skipKey?"Restore "+skipName+(sameKey(skipKey,next)?actionDate:""):next?"Skip "+skipName+actionDate:"No Athan to skip");
 $("skip").classList.toggle("primary",!state?.playing);$("skip").disabled=!connected||settingsFault()||actionBusy||actionUncertain||!domains.settings||!!domains.settings.blocked||(!skipKey&&(!next||firstRun));
 renderPreferences();if(state)renderTimes("times",state.schedule,state.settings);
 text("timezone-note",state?.settings?"Times on the speaker · "+state.settings.timezone:"");text("conflicts",(state?.schedule?.conflicts||[]).join(" "));
 if(preview)renderTimes("preview-times",preview,merge(domains.settings.snapshot.value,draft),true);
 if(firmwareState)renderFirmware(firmwareState);
 renderDraftControls();
}
function showView(view,focus=true){if(firstRun)view="settings";$("today-view").hidden=view!=="today";$("settings-view").hidden=view!=="settings";document.querySelectorAll("[data-view]").forEach(b=>{if(b.dataset.view===view)b.setAttribute("aria-current","page");else b.removeAttribute("aria-current");});if(focus){$("main").focus();scrollTo(0,0);}}
function focusStep(){const id=setupStep===0?"location-heading":setupStep===1?"calculation-heading":"review-heading";$(id).focus();$(id).scrollIntoView({block:"start"});}
function renderDraftControls(){
 const d=domains.settings,blocked=!d||["storage_fault","save_failed"].includes(d?.snapshot.application)||settingsFault();
 document.querySelector(".navigation").hidden=firstRun;document.querySelector(".settings-simple").hidden=firstRun;
 text("settings-intro",firstRun?"Choose a location, review prayer times, then finish setup.":"Everyday preferences save automatically.");
 $("settings-heading").textContent=firstRun?"Welcome to OpenAthan":"Settings";$("prayer-heading").textContent=firstRun?"Set up your speaker":"Prayer times";
 $("setup-progress").hidden=!firstRun;$("setup-progress").querySelectorAll("li").forEach((li,i)=>{if(i===setupStep)li.setAttribute("aria-current","step");else li.removeAttribute("aria-current");});
 $("location-fields").hidden=firstRun&&setupStep!==0;$("calculation-fields").hidden=firstRun&&setupStep!==1;
 $("location-fields").disabled=blocked;$("calculation-fields").disabled=blocked;
 $("setup-back").hidden=!firstRun||setupStep===0;$("setup-next").hidden=!firstRun||setupStep===2;
 $("setup-next").textContent=setupStep===0?"Continue to calculation":"Review timetable";$("setup-next").disabled=blocked||!connected;
 $("preview").hidden=firstRun&&setupStep!==2;$("preview").disabled=blocked||!connected||!!d?.busy||!!d?.blocked;
 $("prayer-preview").hidden=!preview;$("confirm").hidden=!preview;
 const ready=preview?.state==="ready"||preview?.state==="waiting_for_time"&&!helperDraft;
 $("confirm").disabled=blocked||!connected||!ready||preview?.version!==draftVersion||!!d?.busy||!!d?.blocked;
 $("confirm").textContent=firstRun?"Finish setup":"Confirm prayer changes";
 $("discard").hidden=!draftDirty||firstRun;
 $("discard").disabled=!!d?.prayerSaving||!!d?.meta?.sent;
}
function discardDraft(){
 const d=domains.settings;if(!d||d.prayerSaving||d.meta?.sent)return;
 if(d.meta){d.desired=preferencePatch(d.desired);d.pending=preferencePatch(d.pending);d.meta=null;d.groups.delete("prayer");$("prayer-recovery").hidden=true;}
 draftDirty=false;++draftVersion;helperDraft=false;preview=undefined;fillDraft(d.snapshot.value);status("prayer","Saved prayer settings restored","success");render();pump();
}
function enqueue(key,patch,meta=null){
 const d=domains[key];if(!d||key==="settings"&&settingsFault())return;
 d.desired=merge(d.desired,patch);d.pending=merge(d.pending,patch);
 if(meta){d.meta=meta;d.groups.add("prayer");}
 specs[key].groups.forEach(g=>d.groups.add(g));
 if(!connected){d.blocked="uncertain";notify(key,"Not saved · connection lost. Your edits are kept.","warning",[...d.groups]);recovery(key);}
 else if(!d.blocked)notify(key,"Waiting to save…","",[...d.groups]);
 render();pump();
}
function bodyFor(key,before,candidate,meta){const base={schema:1,expected_revision:before.revision};return key==="settings"?{...base,settings:candidate,refresh_timezone:!!meta?.refreshTimezone}:key==="lights"?{...base,settings:candidate}:{...base,...candidate};}
function matches(key,actual,expected,before,meta){
 if(!valueFrom(key,actual)||!Number.isInteger(actual.revision))return false;
 const value=clone(valueFrom(key,actual)),wanted=clone(expected);
 if(key==="settings"&&(wanted.timezone!==before.value.timezone||meta?.refreshTimezone)){delete value.timezone_rules;delete wanted.timezone_rules;}
 return stable(value)===stable(wanted)&&actual.revision>=before.revision&&!(meta?.activate&&actual.setup!=="active")&&!["storage_fault","save_failed"].includes(actual.application);
}
function acknowledge(patch,desired){
 for(const [k,v] of Object.entries(patch)){
  if(v&&typeof v==="object"&&!Array.isArray(v)&&desired[k]&&typeof desired[k]==="object"){
   acknowledge(v,desired[k]);if(!Object.keys(desired[k]).length)delete desired[k];
  }else if(stable(desired[k])===stable(v))delete desired[k];
 }
}
function finishSaved(key,patch,meta,raw,verified=false,playbackAtStart=playbackVersion){
 if(key==="settings")raw=preservePlayback(raw,playbackAtStart);
 const d=domains[key];
 acceptSavedState(key,raw);
 acknowledge(patch,d.desired);
 if(meta&&meta.version===draftVersion){draftDirty=false;helperDraft=false;preview=undefined;fillDraft(raw.settings);if(meta.activate){firstRun=raw.setup==="incomplete";if(!firstRun)showView("today");}}
 if(meta&&meta.version!==draftVersion)status("prayer","Reviewed settings saved · your newer edits remain a draft","warning");
 else if(meta)status("prayer","Prayer settings saved","success");
 notify(key,verified?"Saved values verified after readback":savedMessage(key)[0],verified?"success":savedMessage(key)[1]);
 render();
}
async function pump(){
 if(writeBusy||firmwareBusy||actionUncertain)return;
 if(pendingAction&&!settingsFault()&&!domains.settings?.busy&&!domains.settings?.blocked){const pending=pendingAction;pendingAction=null;writeBusy=true;await runAction(pending);writeBusy=false;pump();return;}
 const key=Object.keys(specs).find(k=>domains[k]&&!(k==="settings"&&settingsFault())&&!settingsFault(domains[k].snapshot)&&!domains[k].blocked&&!domains[k].busy&&Object.keys(domains[k].pending).length);
 if(!key||!connected)return;
 const d=domains[key],patch=clone(d.pending),meta=d.meta,before=clone(d.snapshot),candidate=merge(before.value,patch),groups=[...d.groups],playbackAtStart=playbackVersion;
 if(meta){meta.sent=true;d.prayerSaving=true;}
 d.pending={};d.meta=null;d.groups.clear();d.busy=true;writeBusy=true;++generation;if(firmwareState)renderFirmware(firmwareState);notify(key,"Saving…","",groups);renderDraftControls();
 try{
  const raw=await request(meta?.activate?"/api/activate":specs[key].path,bodyFor(key,before,candidate,meta));
  if(!Number.isInteger(raw.revision)||!valueFrom(key,raw))throw new Error("Saved values could not be confirmed");
  finishSaved(key,patch,meta,raw,false,playbackAtStart);
 }catch(error){
  d.pending=merge(patch,d.pending);if(meta&&!d.meta)d.meta=meta;groups.forEach(g=>d.groups.add(g));
  notify(key,"Save unconfirmed · checking saved state…","warning",groups);
  try{
   const raw=await request(specs[key].read);
   if(matches(key,raw,candidate,before,meta)){
    acknowledge(patch,d.pending);
    if(d.meta===meta)d.meta=null;
    finishSaved(key,patch,meta,raw,true,playbackAtStart);
   }else{
    acceptSavedState(key,key==="settings"?preservePlayback(raw,playbackAtStart):raw);
    d.blocked=error.status===409?"conflict":"failure";
    notify(key,error.status===409?"Changed on another client. Your edits are kept; choose which values to use.":"Couldn’t save. Your edits are kept; retry or use the saved values.","error",groups);
    if(meta)status("prayer","Prayer changes were not confirmed. Your draft is kept.","error");
    recovery(key);
   }
  }catch{
   connected=false;d.blocked="uncertain";notify(key,"Save unconfirmed · reconnect and check saved state before another write.","warning",groups);
   if(meta)status("prayer","Prayer save unconfirmed. Your draft is kept.","warning");
   recovery(key);
  }
 }finally{d.prayerSaving=false;d.busy=false;writeBusy=false;render();pump();}
}
function recovery(key){
 const groups=[...new Set([...specs[key].groups,...(domains[key].meta?["prayer"]:[])])];
 groups.forEach(group=>{
  const el=$(group+"-recovery");el.hidden=false;el.replaceChildren();
  for(const [label,useSaved] of domains[key].blocked==="uncertain"?[["Check saved state",false]]:[["Retry with my edits",false],["Use saved values",true]]){
   const button=document.createElement("button");button.type="button";button.textContent=key==="settings"&&domains[key].meta&&!useSaved&&domains[key].blocked!=="uncertain"?"Review prayer draft":label;button.addEventListener("click",()=>resolve(key,useSaved));el.append(button);
  }
 });
}
async function resolve(key,useSaved){
 const d=domains[key];if(d.busy)return;const playbackAtStart=playbackVersion;d.busy=true;notify(key,"Checking saved state…");renderDraftControls();
 try{
  let raw=await request(specs[key].read);if(key==="settings")raw=preservePlayback(raw,playbackAtStart);const before=clone(d.snapshot),candidate=merge(before.value,d.desired);
  const persisted=matches(key,raw,candidate,before,d.meta);acceptSavedState(key,raw);
  if(key==="settings"&&settingsFault()||settingsFault(raw)){d.blocked="failure";recovery(key);return;}
  if(persisted){const patch=clone(d.desired),meta=d.meta;d.pending={};d.meta=null;finishSaved(key,patch,meta,raw,true);d.blocked=null;}
  else if(d.blocked==="uncertain"){d.blocked="failure";notify(key,"Saved state checked. Your edits are kept; choose which values to use.","warning");recovery(key);}
  else{d.blocked=null;if(!useSaved&&d.meta){d.pending=Object.fromEntries(Object.entries(d.pending).filter(([k])=>["volume","enabled"].includes(k)));d.desired=Object.fromEntries(Object.entries(d.desired).filter(([k])=>["volume","enabled"].includes(k)));d.meta=null;preview=undefined;status("prayer","Your draft is kept. Preview again before confirming.","warning");}if(useSaved){const discardPrayer=!!d.meta;d.desired={};d.pending={};d.meta=null;if(key==="settings"&&discardPrayer){draftDirty=false;helperDraft=false;preview=undefined;fillDraft(raw.settings);status("prayer","Saved prayer settings restored","success");}}}
  if(!d.blocked)for(const group of [...specs[key].groups,"prayer"]){const el=$(group+"-recovery");if(el)el.hidden=true;}
 }catch{d.blocked="uncertain";connected=false;notify(key,"Saved state unavailable. Your edits are kept; reconnect to check.","warning");recovery(key);}
 finally{d.busy=false;render();await refresh();pump();}
}
async function refresh(){
 if(loading||writeBusy)return false;loading=true;const started=generation;
 try{const state=await request("/api/status");if(started!==generation)return false;acceptStatus(state);return true;}
 catch{if(started===generation){connected=false;render();}return false;}finally{loading=false;attempted=true;if(!connected)render();}
}
async function previewDraft(){
 if(!draft||!connected||settingsFault()||domains.settings.busy)return;
 if(!$("prayer-form").reportValidity())return;readDraft();const version=draftVersion,before=clone(domains.settings.snapshot);
 status("prayer","Preparing timetable preview…");$("preview").disabled=true;
 try{
  const data=await request("/api/preview",bodyFor("settings",before,merge(before.value,draft),{refreshTimezone:$("refresh-timezone").checked}));
  if(settingsFault()||version!==draftVersion||stable(calculation(before.value))!==stable(calculation(domains.settings.snapshot.value)))return;
  preview={...data,version};if(firstRun)setupStep=2;
  renderTimes("preview-times",data,merge(domains.settings.snapshot.value,draft),true);
  text("preview-warning",data.state==="waiting_for_time"?helperDraft?"Waiting for time. Preview again after the clock is ready before saving this suggestion.":"You can finish setup now; announcements will wait for a valid clock.":data.state==="invalid_schedule"?"These settings cannot produce a valid schedule. Review location, conventions and offsets.":(data.conflicts||[]).join(" "));
  $("preview-warning").hidden=!$("preview-warning").textContent;text("review-summary","Compare these times with the convention followed by your local mosque.");
  status("prayer",data.state==="ready"?"Preview ready · confirm to save":"Review the preview warning",data.state==="ready"?"":"warning");
  renderDraftControls();if(firstRun)focusStep();
 }catch(error){status("prayer",error.message+". Your draft is kept; try preview again.","error");}finally{renderDraftControls();}
}
async function runAction(action){
 ++generation;const playbackAtStart=playbackVersion;
 try{acceptStatus(preservePlayback(await request(action.restore?"/api/cancel-skip":"/api/skip",{expected_revision:domains.settings.snapshot.revision,occurrence:action.occurrence}),playbackAtStart));status("action",action.name+(action.restore?" restored":" will be skipped"),"success");}
 catch(error){
  try{const state=preservePlayback(await request("/api/status"),playbackAtStart);acceptStatus(state);status("action",error.status===409?"The prayer or settings changed. Check the current prayer before trying again.":"Skip state checked · "+(isSkipped(action.occurrence,state.skip)?"Athan is skipped":"Athan is on"),error.status===409?"warning":"");}
  catch{connected=false;actionUncertain=true;status("action",(action.restore?"Restore":"Skip")+" unconfirmed · reconnect to check","warning");}
 }finally{actionBusy=false;$("action-feedback").hidden=false;render();}
}
function stoppedFeedback(){stopAwaiting=false;status("action","Playback stopped"+(actionUncertain?" · Skip state unconfirmed; refresh to check.":""),actionUncertain?"warning":"success");text("settings-playback-feedback","Playback stopped");}
function acceptStopStatus(state){
 if(typeof state?.playing!=="boolean"||!Number.isInteger(state.revision)||!state.settings&&!settingsFault(state))throw new Error("Stop status is incomplete");
 const order=responseOrders.get(state)||statusOrder;
 if(snapshot&&order<statusOrder)state=snapshot;
 else if(snapshot?.settings&&state.settings&&state.revision<snapshot.revision)state={...snapshot,playing:state.playing};
 acceptStatus(state,undefined,order);return snapshot;
}
async function stopPlayback(){
 if(stopBusy||!connected)return;stopBusy=true;stopAwaiting=true;++generation;++playbackVersion;status("action","Requesting Stop…");text("settings-playback-feedback","Requesting Stop…");$("action-feedback").hidden=false;$("settings-playback-feedback").hidden=false;render();
 try{const state=acceptStopStatus(await request("/api/stop",{}));if(state.playing){stopAwaiting=true;status("action","Stop requested · waiting for playback to stop","warning");text("settings-playback-feedback","Stop requested · waiting for playback to stop");}else stoppedFeedback();}
 catch{try{acceptStopStatus(await request("/api/status"));const message=snapshot.playing?"Playback is still active. Try Stop again.":"Playback stopped · confirmed after readback";status("action",message,snapshot.playing?"warning":"success");text("settings-playback-feedback",message);}catch{connected=false;const message="Stop unconfirmed · reconnect to check. Playback may still be active.";status("action",message,"warning");text("settings-playback-feedback",message);}}
 finally{stopBusy=false;$("action-feedback").hidden=false;$("settings-playback-feedback").hidden=false;render();}
}
function slider(id,key,field){
 const input=$(id);
 input.addEventListener("input",()=>{const d=domains[key];if(!d)return;d.desired=merge(d.desired,{[field]:Number(input.value)});$(id+"-value").value=input.value+"%";if(key==="settings"){const other=id==="volume"?"settings-volume":"volume";$(other).value=input.value;$(other+"-value").value=input.value+"%";}notify(key,"Unsaved · release to save","warning");});
 input.addEventListener("change",()=>{clearTimeout(sliderTimers.get(id));const submit=()=>enqueue(key,{[field]:Number(input.value)});if(document.activeElement===input&&!input.matches(":active"))sliderTimers.set(id,setTimeout(submit,350));else submit();});
}
for(const [id,key,field] of [["volume","settings","volume"],["settings-volume","settings","volume"],["screen-brightness","display","brightness_percent"],["lights-brightness","lights","brightness_percent"]])slider(id,key,field);
for(const p of prayers){const label=document.createElement("label"),input=document.createElement("input");label.className="check";input.type="checkbox";input.id="enabled-"+p;input.disabled=true;input.addEventListener("change",()=>enqueue("settings",{enabled:{[p]:input.checked}}));label.append(input,document.createTextNode(title(p)));$("enabled-prayers").append(label);}
for(const p of events){const label=document.createElement("label"),input=document.createElement("input");input.type="number";input.id="offset-"+p;input.min=-120;input.max=120;input.required=true;label.append(document.createTextNode(title(p)),input);$("offsets").append(label);}
document.querySelectorAll("[data-view]").forEach(button=>button.addEventListener("click",()=>showView(button.dataset.view)));
$("prayer-form").addEventListener("input",markDraft);
$("prayer-form").addEventListener("submit",e=>{e.preventDefault();if(!$("confirm").disabled&&preview&&preview.version===draftVersion)enqueue("settings",clone(draft),{version:draftVersion,refreshTimezone:$("refresh-timezone").checked,activate:firstRun});});
$("preview").addEventListener("click",previewDraft);
$("discard").addEventListener("click",discardDraft);
$("setup-next").addEventListener("click",()=>{if(setupStep===0){if(!["latitude","longitude","timezone"].every(id=>$(id).reportValidity()))return;setupStep=1;renderDraftControls();focusStep();}else previewDraft();});
$("setup-back").addEventListener("click",()=>{setupStep=Math.max(0,setupStep-1);preview=undefined;renderDraftControls();focusStep();});
$("lights-enabled").addEventListener("change",()=>enqueue("lights",{enabled:$("lights-enabled").checked}));
$("time-format").addEventListener("change",()=>enqueue("time_format",{hours:Number($("time-format").value)}));
$("skip").addEventListener("click",()=>{if(actionBusy||actionUncertain||!snapshot)return;actionBusy=true;pendingAction={restore:!!snapshot.skip,occurrence:clone(snapshot.skip||snapshot.next),name:snapshot.skip?title(prayers[snapshot.skip.prayer]):snapshot.next.name};status("action","Sending request…");$("action-feedback").hidden=false;render();pump();});
for(const id of ["stop","settings-stop"])$(id).addEventListener("click",stopPlayback);
$("reconnect").addEventListener("click",async()=>{if(await refresh()){if(actionUncertain){actionUncertain=false;status("action","Skip state checked · review the current prayer");render();pump();}for(const key of Object.keys(domains))if(domains[key].blocked==="uncertain")await resolve(key,false);}if(!timezonesReady)await loadTimezones();});
$("refresh").addEventListener("click",async()=>{await refresh();if(!timezonesReady)await loadTimezones();});
addEventListener("beforeunload",e=>{if(draftDirty){e.preventDefault();e.returnValue="";}});

function locationMessage(value,error=false){status("prayer",value,error?"error":"warning");}
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
proposedLocation=incomingLocation();
const helperUrl=new URL("https://openathan.com/location/");
helperUrl.hash=new URLSearchParams({v:"1",device:location.origin+"/"}).toString();
$("find-location").href=helperUrl.href;
function applyLocationProposal(proposal) {
  if(!proposal)return;
  if(proposal.error){locationMessage("Location link was invalid. Enter your location manually.",true);return;}
  if(draftDirty && !confirm("Replace your unsaved location edits with the suggested location?")){
    locationMessage("Your unsaved edits were kept. You can enter the suggested values manually.");return;
  }
  $("latitude").value=String(proposal.latitude);$("longitude").value=String(proposal.longitude);
  const zoneSupported=!!proposal.timezone && supportedZones.has(proposal.timezone);
  $("timezone").value=zoneSupported?proposal.timezone:"";
  helperDraft=true;
  showView("settings");markDraft();
  const source=proposal.source==="ip"?"Approximate IP location":"Browser location";
  const accuracy=Number.isFinite(proposal.accuracy)
    ?proposal.source==="ip"?` Estimated radius: ${Math.ceil(proposal.accuracy)} km.`:` Reported accuracy: ${Math.ceil(proposal.accuracy)} metres.`:"";
  const timezone=zoneSupported?" Check the timezone.":" Choose a timezone supported by this device.";
  $("location-feedback").textContent=`${source} suggested.${accuracy}${timezone} Preview the timetable before saving.`;
  $("location-feedback").hidden=false;
  locationMessage("Location suggested. Review it and preview the timetable before saving.");
}
function applyPendingLocation() {
  if(!draft || settingsFault() || !timezonesReady || !proposedLocation)return;
  const proposal=proposedLocation;proposedLocation=null;
  applyLocationProposal(proposal);
}
addEventListener("hashchange",()=>{
  proposedLocation=incomingLocation();
  applyPendingLocation();
});

async function loadTimezones(){if(timezonesLoading)return;timezonesLoading=true;try{const data=await request("/api/timezones");for(const name of data.names){supportedZones.add(name);$("zones").append(new Option(name,name));}timezonesReady=true;applyPendingLocation();}catch{locationMessage("Timezone list unavailable. Your suggestion and manual edits are kept; use Refresh to retry.",true);}finally{timezonesLoading=false;}}
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
  text("firmware-version",`Installed: ${state.version}${state.queued_version?` · Queued: ${state.queued_version}`:state.available?` · Available: ${state.available.version}`:""}`);
  const progress=state.state==="downloading" && state.total?` ${Math.floor(100*state.received/state.total)}%`:"";
  text("firmware-status",(state.supported===false?"This speaker needs a maintainer USB update before Wi-Fi installation. ":"")+(firmwareMessages[state.state] || "Reading update status…")+progress+(state.error?` ${state.error}`:""));
  if(firmwareUncertain)text("firmware-status","The response was lost. Waiting to read update status before allowing another action. Keep the speaker powered.");
  text("firmware-last-check",state.last_check?`Last checked: ${new Date(state.last_check*1000).toLocaleString(undefined,{hour12:timeSnapshot?.hours===12,timeZone:"UTC"})+" UTC"}`:"No successful update check yet.");
  const working=["checking","queued","downloading","verifying","restarting"].includes(state.state);
  $("firmware-check").disabled=firmwareBusy || writeBusy || !connected || firmwareUncertain || working || state.state==="storage_fault";
  $("firmware-install").hidden=!state.available || working;
  $("firmware-install").disabled=firmwareBusy || writeBusy || !connected || firmwareUncertain || state.state==="storage_fault" || state.supported===false;
  $("firmware-cancel").hidden=!["queued","downloading","verifying"].includes(state.state);
  $("firmware-cancel").disabled=firmwareBusy || writeBusy || !connected || firmwareUncertain;
  $("firmware-notes").hidden=!state.available;
  if(state.available)$("firmware-notes").href=`https://github.com/OpenAthan-Project/openathan/releases/tag/${encodeURIComponent(state.available.version)}`;
}
async function firmwareAction(action) {
  if(firmwareBusy || firmwareUncertain || !firmwareState || writeBusy)return;
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
  } finally {firmwareBusy=false;if(firmwareState)renderFirmware(firmwareState);pump();}
}
for(const action of ["check","install","cancel"])$("firmware-"+action).addEventListener("click",()=>firmwareAction(action));
setInterval(async()=>{
  if(document.hidden || firmwareBusy || !firmwareState)return;
  const started=firmwareGeneration;
  try {const state=await request("/api/firmware");if(started===firmwareGeneration && !firmwareBusy)renderFirmware(state,true);}
  catch {if(firmwareExpected || ["downloading","verifying","restarting"].includes(firmwareState.state))$("firmware-status").textContent="Waiting for the speaker to reconnect. Keep it powered.";}
},3000);

render();
(async()=>{await refresh();await loadTimezones();})();
setInterval(()=>{if(!document.hidden)refresh();},5000);

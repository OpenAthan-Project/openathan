"use strict";
const $ = (id) => document.getElementById(id);
const clone = (value) => structuredClone(value);
const prayers = ["fajr", "dhuhr", "asr", "maghrib", "isha"];
const events = ["fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"];
const title = (value) => value[0].toUpperCase() + value.slice(1);
const specs = {
  settings: { path: "/api/settings", read: "/api/status", groups: ["volume", "preferences"] },
  display: { path: "/api/display", read: "/api/display", groups: ["screen"] },
  lights: { path: "/api/lights", read: "/api/lights", groups: ["lights"] },
  time_format: { path: "/api/time-format", read: "/api/time-format", groups: ["format"] }
};
const domains = {},
  renderKeys = new Map(),
  supportedZones = new Set();
const observedStatus = { data: null, order: 0 },
  firmware = { data: null, order: 0 };
let requestOrder = 0,
  contactOrder = 0,
  editVersion = 0,
  activeWrite = null;
let snapshot,
  connected = false,
  loading = false,
  attempted = false;
let draft,
  draftDirty = false,
  draftVersion = 0,
  preview,
  previewOperation;
let helperDraft = false,
  firstRun = false,
  setupStep = 0;
let actionBusy = false,
  actionUncertain = false,
  pendingAction,
  actionOrder = 0;
let stopBusy = false,
  stopAwaiting = false;
let timezonesReady = false,
  timezonesLoading = false,
  proposedLocation;
let firmwareBusy = false,
  firmwareUncertain = false,
  firmwareActionOrder = 0;

function merge(base, patch) {
  const out = clone(base);
  for (const [key, value] of Object.entries(patch)) {
    out[key] =
      value && typeof value === "object" && !Array.isArray(value)
        ? merge(out[key] || {}, value)
        : value;
  }
  return out;
}
function stable(value) {
  return JSON.stringify(value, (_, item) =>
    item && typeof item === "object" && !Array.isArray(item)
      ? Object.fromEntries(
          Object.keys(item)
            .sort()
            .map((key) => [key, item[key]])
        )
      : item
  );
}
function text(id, value) {
  if ($(id).textContent !== value) $(id).textContent = value;
}
function status(group, value, type = "") {
  text(group + "-feedback", value);
  $(group + "-feedback").className = "feedback" + (type ? " " + type : "");
}
function notify(key, value, type = "", groups = specs[key].groups) {
  groups.forEach((group) => status(group, value, type));
}
function writing() {
  return activeWrite !== null;
}
function storageFault(state) {
  return (
    state?.setup === "storage_fault" ||
    ["storage_fault", "save_failed"].includes(state?.application)
  );
}
function settingsFault() {
  return storageFault(domains.settings);
}
function formatHours() {
  return domains.time_format?.confirmed?.value.hours;
}
function calculation(value) {
  const out = clone(value);
  delete out.volume;
  delete out.enabled;
  return out;
}
function valueFrom(key, state) {
  if (!state) return null;
  if (key === "settings" || key === "lights") return state.settings || null;
  if (key === "display")
    return Number.isFinite(state.brightness_percent)
      ? { brightness_percent: state.brightness_percent }
      : null;
  return [12, 24].includes(state.hours) ? { hours: state.hours } : null;
}
function fieldPatch(field, value) {
  return field.split(".").reduceRight((patch, key) => ({ [key]: patch }), value);
}
function editPatch(edits) {
  let patch = {};
  for (const [field, edit] of edits) patch = merge(patch, fieldPatch(field, edit.value));
  return patch;
}
function current(key) {
  const d = domains[key];
  return d?.confirmed ? merge(d.confirmed.value, editPatch(d.edits)) : null;
}
function releasedEdits(d) {
  return new Map([...d.edits].filter(([, edit]) => edit.ready));
}
function hasQueuedWork(d) {
  return !!d.review || releasedEdits(d).size > 0;
}
function groupsFor(d, review = d.review) {
  return [...specs[d.key].groups, ...(review ? ["prayer"] : [])];
}
function cancelTimer(edit) {
  if (edit?.timer) clearTimeout(edit.timer);
  if (edit) edit.timer = null;
}
function editField(key, field, value, ready = false, control) {
  const d = domains[key];
  if (!d?.confirmed || storageFault(d)) return;
  cancelTimer(d.edits.get(field));
  const edit = { value, version: ++editVersion, ready, timer: null, control };
  d.edits.set(field, edit);
  return edit;
}
function captureEdits(d, edits = d.edits) {
  return new Map(
    [...edits].map(([field, edit]) => [
      field,
      { value: edit.value, version: edit.version, ready: edit.ready }
    ])
  );
}
function acknowledgeEdits(d, captured, useSaved = false) {
  for (const [field, edit] of captured) {
    const latest = d.edits.get(field);
    if (latest?.version === edit.version && (edit.ready || useSaved)) {
      cancelTimer(latest);
      d.edits.delete(field);
    }
  }
}
function sameKey(a, b) {
  return !!a && !!b && a.day === b.day && a.prayer === b.prayer;
}
function isSkipped(next, skip) {
  return sameKey(next, skip) || sameKey(next?.shared_with, skip);
}
function dateLabel(value) {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(value || "")) return "";
  return new Intl.DateTimeFormat(undefined, {
    weekday: "long",
    day: "numeric",
    month: "long",
    timeZone: "UTC"
  }).format(new Date(value + "T12:00:00Z"));
}
function localTime(value) {
  const match = /^(\d{4}-\d{2}-\d{2}) ([0-2]\d):([0-5]\d)$/.exec(value || "");
  if (!match || +match[2] > 23) return "Unavailable";
  return formatHours() === 12
    ? (+match[2] % 12 || 12) + ":" + match[3] + (+match[2] < 12 ? " AM" : " PM")
    : match[2] + ":" + match[3];
}
async function request(path, body) {
  const order = ++requestOrder;
  if (["/api/skip", "/api/cancel-skip"].includes(path)) actionOrder = order;
  if (path.startsWith("/api/firmware/")) firmwareActionOrder = order;
  try {
    const response = await fetch(path, {
      method: body === undefined ? "GET" : "POST",
      credentials: "same-origin",
      cache: "no-store",
      headers: body === undefined ? {} : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: AbortSignal.timeout(12000)
    });
    let data;
    try {
      data = await response.json();
    } catch {
      throw new Error("The response could not be confirmed");
    }
    if (!response.ok) {
      const error = new Error(data.error || "Device request failed");
      error.status = response.status;
      throw error;
    }
    return { data, order };
  } catch (error) {
    error.order = order;
    throw error;
  }
}
function connectionFailure(error) {
  if (!error.stale && error.order >= contactOrder) {
    connected = false;
  }
}
function invalidatePreview() {
  preview = undefined;
  previewOperation = undefined;
}
function fillDraft(value, fresh = false) {
  draft = calculation(value);
  if (fresh) {
    draft.latitude = "";
    draft.longitude = "";
    draft.timezone = "";
    draft.method = "";
  }
  for (const [id, key] of Object.entries({
    latitude: "latitude",
    longitude: "longitude",
    timezone: "timezone",
    method: "method",
    asr: "asr_method",
    "high-latitude": "high_latitude"
  }))
    $(id).value = draft[key];
  for (const prayer of events) $("offset-" + prayer).value = draft.offsets[prayer];
  $("refresh-timezone").checked = false;
}
function readDraft() {
  for (const [id, key] of Object.entries({
    latitude: "latitude",
    longitude: "longitude",
    timezone: "timezone",
    method: "method",
    asr: "asr_method",
    "high-latitude": "high_latitude"
  })) {
    draft[key] = ["latitude", "longitude"].includes(id)
      ? $(id).value === ""
        ? ""
        : Number($(id).value)
      : $(id).value.trim();
  }
  draft.offsets = Object.fromEntries(
    events.map((prayer) => [prayer, Number($("offset-" + prayer).value)])
  );
}
function markDraft() {
  readDraft();
  draftDirty = true;
  ++draftVersion;
  invalidatePreview();
  status("prayer", "Draft · preview before saving", "warning");
  renderDraftControls();
}

// All response paths share this policy. Revisions order durable values; request
// order independently orders application state, playback, capabilities and faults.
function acceptDomain(key, data, order) {
  if (!data || !Number.isInteger(data.revision)) return { stale: true };
  const fault = storageFault(data),
    value = valueFrom(key, data);
  if (!fault && !value) return { stale: true };
  let d = domains[key];
  if (!d)
    d = domains[key] = {
      key,
      confirmed: null,
      order: 0,
      application: null,
      edits: new Map(),
      review: null,
      operation: null,
      blocked: null
    };
  const older = order < d.order;
  const lower = d.confirmed && !fault && data.revision < d.confirmed.revision && !storageFault(d);
  const stale = !!(lower || older);
  if (lower || (older && (fault || storageFault(d)))) return { stale: true, lower: !!lower };
  const previous = d.confirmed;
  if (
    value &&
    (!fault || (!previous && data.revision > 0)) &&
    (!older || !previous || data.revision > previous.revision)
  ) {
    d.confirmed = { revision: data.revision, value: clone(value) };
    const changed = previous && data.revision !== previous.revision;
    if (
      changed &&
      key === "settings" &&
      stable(calculation(value)) !== stable(calculation(previous.value))
    ) {
      invalidatePreview();
      if (draftDirty)
        status(
          "prayer",
          "Saved prayer settings changed. Your draft is kept; preview again.",
          "warning"
        );
      else if (!firstRun) fillDraft(value);
    }
    if (changed && !d.operation && (d.edits.size || d.review) && !d.blocked) {
      d.blocked = "conflict";
      notify(
        key,
        "Changed on another client. Your edits are kept; choose which values to use.",
        "error"
      );
      recovery(key);
    }
  }
  if (!older) {
    d.order = order;
    d.application = data.application;
    d.setup = data.setup;
    d.supported = data.supported;
    d.mode = data.mode;
    d.observedRevision = data.revision;
  }
  return { stale, lower: !!lower, fault };
}
function domainData(key) {
  const d = domains[key];
  if (!d) return undefined;
  const value = d.confirmed?.value;
  const data = {
    schema: 1,
    revision: storageFault(d) ? d.observedRevision : (d.confirmed?.revision ?? 0),
    application: d.application
  };
  if (key === "settings") return { ...data, setup: d.setup, ...(value ? { settings: value } : {}) };
  if (d.supported !== undefined) data.supported = d.supported;
  if (d.mode !== undefined) data.mode = d.mode;
  return { ...data, ...(value ? (key === "lights" ? { settings: value } : value) : {}) };
}
function projectSnapshot() {
  if (!observedStatus.data) return;
  snapshot = { ...observedStatus.data, ...domainData("settings") };
  for (const key of ["display", "lights", "time_format"]) snapshot[key] = domainData(key);
  snapshot.firmware = firmware.data;
}
function acceptResponse({ data, order }, resource = "status") {
  const accepted = {};
  const initial = !snapshot,
    wasFault = settingsFault();
  if (resource === "status") {
    if (!Number.isInteger(data?.revision) || (!data.settings && !storageFault(data)))
      throw new Error("Device status is incomplete");
    accepted.settings = acceptDomain("settings", data, order);
    for (const key of ["display", "lights", "time_format"])
      if (data[key]) accepted[key] = acceptDomain(key, data[key], order);
    if (order >= observedStatus.order) {
      // Durable settings are projected from their domain, never from this copy.
      const {
        settings,
        revision,
        application,
        display,
        lights,
        time_format,
        firmware: update,
        ...operational
      } = data;
      observedStatus.data = operational;
      observedStatus.order = order;
      if (actionUncertain && order >= actionOrder) {
        actionUncertain = false;
        status("action", "Skip state checked · review the current prayer");
        queueMicrotask(pump);
      }
    }
  } else if (resource !== "firmware") accepted[resource] = acceptDomain(resource, data, order);
  const update = resource === "firmware" ? data : resource === "status" ? data.firmware : null;
  if (update && order >= firmware.order) {
    firmware.data = update;
    firmware.order = order;
    if (order >= firmwareActionOrder) firmwareUncertain = false;
  }
  // A valid reply establishes contact even after another read failed. Failed
  // reads contain no operational observation; only successful replies order it.
  contactOrder = Math.max(contactOrder, order);
  connected = true;
  projectSnapshot();
  if (snapshot) {
    if (stopAwaiting && snapshot.playing === false) stoppedFeedback();
    firstRun = snapshot.setup === "incomplete";
    if (initial) {
      if (snapshot.settings) fillDraft(snapshot.settings, firstRun && snapshot.revision === 1);
      status(
        "prayer",
        firstRun ? "Setup not finished · choose your location" : "Saved prayer settings",
        firstRun ? "" : "success"
      );
      showView(firstRun ? "settings" : "today", false);
    }
    if (firstRun && $("settings-view").hidden) showView("settings");
    if (snapshot.settings && !draft)
      fillDraft(snapshot.settings, firstRun && snapshot.revision === 1);
    if (wasFault && !settingsFault())
      status(
        "prayer",
        draftDirty
          ? "Saved storage recovered. Your draft is kept; preview before saving."
          : "Saved prayer settings",
        draftDirty ? "warning" : "success"
      );
    if (settingsFault()) {
      invalidatePreview();
      status(
        "prayer",
        "Saved prayer storage is unavailable. Your draft is kept; restart and check saved state.",
        "error"
      );
    }
  }
  render();
  applyPendingLocation();
  return accepted;
}
function acceptSavedResponse(key, response) {
  const result = acceptResponse(response, key === "settings" ? "status" : key)[key];
  if (!result || result.stale || (!connected && response.order < contactOrder)) {
    const error = new Error("Newer saved state requires fresh readback");
    error.stale = true;
    error.order = response.order;
    if (result?.lower) error.status = 409;
    throw error;
  }
  return response.data;
}
function savedMessage(key) {
  const app = domains[key].application;
  if (app === "storage_fault" || app === "save_failed")
    return [
      "Saved storage is unavailable. Your edits are kept; restart and check saved state.",
      "error"
    ];
  if (app === "output_unavailable")
    return [
      key === "display"
        ? "Saved · backlight unavailable. The device will retry."
        : "Saved · lights unavailable. Scheduling continues.",
      "warning"
    ];
  if (app === "volume_pending") return ["Saved · applying volume…", "warning"];
  if (app === "volume_failed")
    return ["Saved · volume could not be applied. Automatic playback is paused.", "error"];
  return ["Saved to speaker", "success"];
}
function renderPreferences() {
  for (const [key, ids] of Object.entries({
    settings: ["volume", "settings-volume"],
    display: ["screen-brightness"],
    lights: ["lights-brightness"],
    time_format: []
  })) {
    const value = current(key);
    if (!value) {
      for (const id of ids) {
        $(id).disabled = true;
        $(id + "-value").value = "—";
      }
      continue;
    }
    for (const id of ids) {
      const v = key === "settings" ? value.volume : value.brightness_percent;
      $(id).value = v;
      $(id + "-value").value = v + "%";
    }
  }
  const s = current("settings");
  if (s) prayers.forEach((p) => ($("enabled-" + p).checked = s.enabled[p]));
  $("lights-enabled").indeterminate = !current("lights");
  if (current("lights")) $("lights-enabled").checked = current("lights").enabled;
  else {
    $("lights-enabled").disabled = true;
    if (snapshot?.lights?.supported && !domains.lights)
      status(
        "lights",
        "Saved light preferences are unavailable. Restart the speaker and check saved state.",
        "error"
      );
  }
  $("time-format").value = current("time_format")?.hours ?? "";
  for (const [key, d] of Object.entries(domains)) {
    const fault =
      ["storage_fault", "save_failed"].includes(d.application) ||
      (key === "settings" && settingsFault());
    if (
      (fault && key !== "settings") ||
      (!d.operation && !d.blocked && !d.edits.size && !(key === "settings" && settingsFault()))
    )
      notify(key, ...savedMessage(key));
    else if (!fault && !d.operation && !d.blocked)
      notify(key, hasQueuedWork(d) ? "Waiting to save…" : "Unsaved · release to save", "warning");
    if (key !== "settings")
      for (const group of specs[key].groups)
        $(group + "-recovery")
          .querySelectorAll("button")
          .forEach((b) => (b.disabled = fault));
    const elements =
      key === "settings"
        ? ["volume", "settings-volume", ...prayers.map((p) => "enabled-" + p)]
        : key === "display"
          ? ["screen-brightness"]
          : key === "lights"
            ? ["lights-enabled", "lights-brightness"]
            : ["time-format"];
    elements.forEach((id) => ($(id).disabled = fault || firstRun));
  }
  if (settingsFault()) {
    notify(
      "settings",
      "Saved prayer storage is unavailable. Your edits are kept; restart and check saved state.",
      "error"
    );
    for (const group of ["volume", "preferences", "prayer"])
      $(group + "-recovery")
        .querySelectorAll("button")
        .forEach((b) => (b.disabled = true));
  } else
    for (const group of ["volume", "preferences", "prayer"])
      $(group + "-recovery")
        .querySelectorAll("button")
        .forEach((b) => (b.disabled = false));
  $("hardware-group").hidden = !snapshot?.display?.supported && !snapshot?.lights?.supported;
  $("screen-controls").hidden = !snapshot?.display?.supported;
  $("light-controls").hidden = !snapshot?.lights?.supported;
  $("format-group").hidden = !snapshot?.time_format;
}
function renderTimes(id, schedule, settings, isPreview = false) {
  const next = snapshot?.clock_ready && snapshot?.setup === "active" ? snapshot.next : null;
  const signature = stable([
    schedule,
    settings?.enabled,
    isPreview ? null : next,
    isPreview ? null : snapshot?.skip,
    formatHours(),
    isPreview ? false : settingsFault()
  ]);
  if (renderKeys.get(id) === signature) return;
  renderKeys.set(id, signature);
  const rows = (schedule?.times || []).map((item) => {
    const p = item.name?.toLowerCase(),
      row = document.createElement("div"),
      dt = document.createElement("dt"),
      dd = document.createElement("dd"),
      small = document.createElement("small");
    row.className = "time-row";
    row.dataset.prayer = p;
    dt.textContent = item.name;
    if (
      !isPreview &&
      next &&
      prayers[next.prayer] === p &&
      Number.isFinite(item.utc) &&
      item.utc === next.utc
    ) {
      row.setAttribute("aria-current", "time");
      const label = document.createElement("span");
      label.className = "visually-hidden";
      label.textContent = ", next prayer";
      dt.append(label);
    }
    small.textContent =
      p === "sunrise"
        ? "No Athan"
        : !settings?.enabled?.[p]
          ? "Athan off"
          : !isPreview && isSkipped(next, snapshot?.skip) && row.hasAttribute("aria-current")
            ? "Skipped"
            : "Athan on";
    dt.append(small);
    dd.textContent = localTime(item.local);
    if (isPreview) {
      const before = snapshot?.schedule?.times?.find((t) => t.name === item.name);
      if (before?.local && before.local !== item.local) {
        const label = document.createElement("small");
        label.className = "before-time";
        label.textContent = "Was " + localTime(before.local);
        dd.append(label);
      }
    }
    row.append(dt, dd);
    return row;
  });
  $(id).replaceChildren(...rows);
  if (!rows.length) {
    const p = document.createElement("p");
    p.className = "hint";
    p.textContent =
      !isPreview && settingsFault()
        ? "The timetable is unavailable while saved prayer storage needs attention."
        : schedule?.state === "waiting_for_time"
          ? "A timetable will appear after the speaker’s clock is ready."
          : schedule?.state === "invalid_schedule"
            ? "These settings cannot produce a valid timetable. Review prayer settings."
            : "Finish setup to see your timetable.";
    $(id).append(p);
  }
}
function render() {
  const state = snapshot,
    next = state?.clock_ready && state?.setup === "active" ? state.next : null,
    skipped = isSkipped(next, state?.skip);
  $("connection-banner").hidden = connected || (!snapshot && !attempted);
  text("connection-heading", snapshot ? "Connection lost" : "Speaker unavailable");
  text(
    "connection-detail",
    snapshot
      ? "Displayed information is stale. The speaker’s current playback state is unknown."
      : "Device status is unavailable. Check the speaker and connection, then reconnect."
  );
  text("address", state?.hostname || "Connecting to speaker");
  const date = state?.clock_ready ? dateLabel(state.local_date) : "";
  text("device-date", date);
  $("device-date").hidden = !date;
  $("test-banner").hidden = state?.test_mode !== true;
  text("next-name", next?.name || "—");
  text("next-time", next ? localTime(next.local) : "—");
  let nextDate = $("next-date");
  if (!nextDate) {
    nextDate = document.createElement("p");
    nextDate.id = "next-date";
    nextDate.className = "next-date";
    $("next-time").after(nextDate);
  }
  const onTable =
    next &&
    state.schedule?.times?.some(
      (t) =>
        Number.isFinite(t.utc) &&
        t.utc === next.utc &&
        t.name?.toLowerCase() === prayers[next.prayer]
    );
  text("next-date", next && !onTable ? dateLabel(next.local?.slice(0, 10)) : "");
  nextDate.hidden = !nextDate.textContent;
  const readiness = !state
    ? "Connecting to speaker"
    : !connected
      ? "Connection lost · stale"
      : state.playing
        ? "Playing"
        : settingsFault()
          ? "Saved storage unavailable"
          : actionUncertain
            ? "Skip state unconfirmed"
            : state.application === "volume_failed"
              ? "Automatic playback paused"
              : state.application === "volume_pending"
                ? "Applying volume"
                : !state.clock_ready
                  ? "Waiting for time"
                  : firstRun
                    ? "Finish setup to enable Athan"
                    : skipped
                      ? "Athan skipped" +
                        (state.local_date && next.local?.slice(0, 10) !== state.local_date
                          ? " on " + next.local.slice(0, 10)
                          : " today")
                      : state.automatic_ready
                        ? "Ready to play"
                        : "Waiting for device readiness";
  text("readiness-text", readiness);
  $("readiness").style.color =
    state?.automatic_ready && connected && !skipped ? "var(--good)" : "var(--amber)";
  const detail =
    !connected && state
      ? "Last observed: " + (state.playing ? "playing" : "idle") + ". Playback may still be active."
      : settingsFault()
        ? (state.playing ? "Playback is active. " : "") +
          "Saved prayer storage is unavailable. Restart the speaker and check saved state."
        : state?.playing
          ? "Playback is active on the speaker."
          : state?.application === "volume_failed"
            ? "Volume could not be applied. Check the speaker and saved volume."
            : state && !state.clock_ready
              ? "Announcements will wait until the device clock is valid."
              : state?.scheduler_fault && state.scheduler_fault !== "none"
                ? "Device needs attention: " +
                  state.scheduler_fault +
                  ". Check the speaker and prayer settings."
                : "";
  text("state-detail", detail);
  $("state-detail").hidden = !detail;
  for (const id of ["stop", "settings-stop"]) {
    $(id).hidden = !state?.playing;
    $(id).disabled = !connected || stopBusy;
  }
  $("settings-playing").hidden = !state?.playing;
  $("settings-playing").querySelector(".hint").textContent = connected
    ? "Playback is active on the speaker."
    : "Last observed playing · connection lost.";
  const skipKey = state?.skip,
    skipName = skipKey ? title(prayers[skipKey.prayer]) : next?.name;
  const actionDate =
    next && state.local_date && next.local?.slice(0, 10) !== state.local_date
      ? " on " + next.local.slice(0, 10)
      : " today";
  text(
    "skip",
    skipKey
      ? "Restore " + skipName + (sameKey(skipKey, next) ? actionDate : "")
      : next
        ? "Skip " + skipName + actionDate
        : "No Athan to skip"
  );
  $("skip").classList.toggle("primary", !state?.playing);
  $("skip").disabled =
    !connected ||
    settingsFault() ||
    actionBusy ||
    actionUncertain ||
    !domains.settings ||
    !!domains.settings.blocked ||
    (!skipKey && (!next || firstRun));
  renderPreferences();
  if (state) renderTimes("times", state.schedule, state.settings);
  text("timezone-note", state?.settings ? "Times on the speaker · " + state.settings.timezone : "");
  text("conflicts", (state?.schedule?.conflicts || []).join(" "));
  if (preview)
    renderTimes("preview-times", preview, merge(domains.settings.confirmed.value, draft), true);
  if (firmware.data) renderFirmware(firmware.data);
  renderDraftControls();
}
function showView(view, focus = true) {
  if (firstRun) view = "settings";
  $("today-view").hidden = view !== "today";
  $("settings-view").hidden = view !== "settings";
  document.querySelectorAll("[data-view]").forEach((b) => {
    if (b.dataset.view === view) b.setAttribute("aria-current", "page");
    else b.removeAttribute("aria-current");
  });
  if (focus) {
    $("main").focus();
    scrollTo(0, 0);
  }
}
function focusStep() {
  const id =
    setupStep === 0
      ? "location-heading"
      : setupStep === 1
        ? "calculation-heading"
        : "review-heading";
  $(id).focus();
  $(id).scrollIntoView({ block: "start" });
}
function renderDraftControls() {
  const d = domains.settings,
    blocked =
      !d?.confirmed || ["storage_fault", "save_failed"].includes(d?.application) || settingsFault();
  document.querySelector(".navigation").hidden = firstRun;
  document.querySelectorAll(".settings-simple").forEach((el) => (el.hidden = firstRun));
  text(
    "settings-intro",
    firstRun
      ? "Choose a location, review prayer times, then finish setup."
      : "Everyday preferences save automatically."
  );
  $("settings-heading").textContent = firstRun ? "Welcome to OpenAthan" : "Settings";
  $("prayer-heading").textContent = firstRun ? "Set up your speaker" : "Prayer times";
  $("setup-progress").hidden = !firstRun;
  $("setup-progress")
    .querySelectorAll("li")
    .forEach((li, i) => {
      if (i === setupStep) li.setAttribute("aria-current", "step");
      else li.removeAttribute("aria-current");
    });
  $("location-fields").hidden = firstRun && setupStep !== 0;
  $("calculation-fields").hidden = firstRun && setupStep !== 1;
  $("location-fields").disabled = blocked;
  $("calculation-fields").disabled = blocked;
  $("setup-back").hidden = !firstRun || setupStep === 0;
  $("setup-next").hidden = !firstRun || setupStep === 2;
  $("setup-next").textContent = setupStep === 0 ? "Continue to calculation" : "Review timetable";
  $("setup-next").disabled = blocked || !connected || !!previewOperation;
  $("preview").hidden = firstRun && setupStep !== 2;
  $("preview").disabled =
    blocked || !connected || !!d?.operation || !!d?.blocked || !!previewOperation;
  $("prayer-preview").hidden = !preview;
  $("confirm").hidden = !preview;
  const ready =
    preview?.state === "ready" || (preview?.state === "waiting_for_time" && !helperDraft);
  $("confirm").disabled =
    blocked ||
    !connected ||
    !ready ||
    preview?.version !== draftVersion ||
    !!d?.operation ||
    !!d?.blocked ||
    !!previewOperation;
  $("confirm").textContent = firstRun ? "Finish setup" : "Confirm prayer changes";
  $("discard").hidden = !draftDirty || firstRun;
  $("discard").disabled = !!d?.review?.sent;
}

function discardDraft() {
  const d = domains.settings;
  if (!d?.confirmed || d.review?.sent) return;
  d.review = null;
  $("prayer-recovery").hidden = true;
  draftDirty = false;
  ++draftVersion;
  helperDraft = false;
  invalidatePreview();
  fillDraft(d.confirmed.value);
  status("prayer", "Saved prayer settings restored", "success");
  render();
  pump();
}
function queueSave(key) {
  const d = domains[key];
  if (!connected) {
    d.blocked = "uncertain";
    notify(key, "Not saved · connection lost. Your edits are kept.", "warning", groupsFor(d));
    recovery(key);
  } else if (!d.blocked) notify(key, "Waiting to save…", "", groupsFor(d));
  render();
  pump();
}
function enqueue(key, patch, meta = null) {
  const d = domains[key];
  if (!d?.confirmed || storageFault(d)) return;
  if (meta) d.review = { ...meta, patch: clone(patch), sent: false };
  else
    for (const [field, value] of Object.entries(patch)) {
      if (key === "settings" && field === "enabled") {
        for (const [prayer, enabled] of Object.entries(value))
          editField(key, "enabled." + prayer, enabled, true);
      } else editField(key, field, value, true);
    }
  queueSave(key);
}
function bodyFor(key, before, candidate, review) {
  const base = { schema: 1, expected_revision: before.revision };
  return key === "settings"
    ? { ...base, settings: candidate, refresh_timezone: !!review?.refreshTimezone }
    : key === "lights"
      ? { ...base, settings: candidate }
      : { ...base, ...candidate };
}
function matches(key, actual, expected, before, review) {
  if (!valueFrom(key, actual) || !Number.isInteger(actual.revision)) return false;
  const value = clone(valueFrom(key, actual)),
    wanted = clone(expected);
  if (
    key === "settings" &&
    (wanted.timezone !== before.value.timezone || review?.refreshTimezone)
  ) {
    delete value.timezone_rules;
    delete wanted.timezone_rules;
  }
  return (
    stable(value) === stable(wanted) &&
    actual.revision >= before.revision &&
    !(review?.activate && actual.setup !== "active") &&
    !storageFault(actual)
  );
}
function finishSaved(key, operation, response, verified = false) {
  const raw = acceptSavedResponse(key, response),
    d = domains[key],
    review = operation.review;
  if (!matches(key, raw, operation.candidate, operation.before, review))
    throw new Error("Saved values could not be confirmed");
  acknowledgeEdits(d, operation.edits);
  if (d.review === review) d.review = null;
  if (review && review.version === draftVersion) {
    draftDirty = false;
    helperDraft = false;
    invalidatePreview();
    fillDraft(d.confirmed.value);
    if (review.activate && !firstRun) showView("today");
  }
  if (review)
    status(
      "prayer",
      review.version === draftVersion
        ? "Prayer settings saved"
        : "Reviewed settings saved · your newer edits remain a draft",
      review.version === draftVersion ? "success" : "warning"
    );
  const [message, type] = savedMessage(key);
  if (d.edits.size || d.review)
    notify(key, hasQueuedWork(d) ? "Waiting to save…" : "Unsaved · release to save", "warning");
  else
    notify(
      key,
      verified && type === "success" ? "Saved values verified after readback" : message,
      type
    );
}
async function pump() {
  if (writing() || firmwareBusy || actionUncertain) return;
  if (
    pendingAction &&
    !settingsFault() &&
    !domains.settings?.operation &&
    !domains.settings?.blocked
  ) {
    const action = pendingAction;
    pendingAction = null;
    activeWrite = action;
    try {
      await runAction(action);
    } finally {
      activeWrite = null;
      pump();
    }
    return;
  }
  const key = Object.keys(specs).find((key) => {
    const d = domains[key];
    return d?.confirmed && !storageFault(d) && !d.blocked && !d.operation && hasQueuedWork(d);
  });
  if (!key || !connected) return;
  const d = domains[key],
    before = clone(d.confirmed),
    edits = captureEdits(d, releasedEdits(d)),
    review = d.review;
  const patch = merge(review?.patch || {}, editPatch(edits));
  const operation = { kind: "save", before, edits, review, candidate: merge(before.value, patch) };
  if (review) review.sent = true;
  d.operation = operation;
  activeWrite = operation;
  if (firmware.data) renderFirmware();
  notify(key, "Saving…", "", groupsFor(d, review));
  renderDraftControls();
  try {
    const response = await request(
      review?.activate ? "/api/activate" : specs[key].path,
      bodyFor(key, before, operation.candidate, review)
    );
    finishSaved(key, operation, response);
  } catch (error) {
    notify(key, "Save unconfirmed · checking saved state…", "warning", groupsFor(d, review));
    try {
      const response = await request(specs[key].read);
      if (matches(key, response.data, operation.candidate, before, review))
        finishSaved(key, operation, response, true);
      else {
        acceptSavedResponse(key, response);
        d.blocked = error.status === 409 ? "conflict" : "failure";
        notify(
          key,
          error.status === 409
            ? "Changed on another client. Your edits are kept; choose which values to use."
            : "Couldn’t save. Your edits are kept; retry or use the saved values.",
          "error",
          groupsFor(d, review)
        );
        if (review)
          status("prayer", "Prayer changes were not confirmed. Your draft is kept.", "error");
        recovery(key);
      }
    } catch (readbackError) {
      connectionFailure(readbackError);
      d.blocked = "uncertain";
      notify(
        key,
        "Save unconfirmed · reconnect and check saved state before another write.",
        "warning",
        groupsFor(d, review)
      );
      if (review) status("prayer", "Prayer save unconfirmed. Your draft is kept.", "warning");
      recovery(key);
    }
  } finally {
    d.operation = null;
    activeWrite = null;
    render();
    pump();
  }
}
function recovery(key) {
  const d = domains[key];
  for (const group of groupsFor(d)) {
    const element = $(group + "-recovery");
    element.hidden = false;
    element.replaceChildren();
    const choices =
      d.blocked === "uncertain"
        ? [["Check saved state", false]]
        : [
            ["Retry with my edits", false],
            ["Use saved values", true]
          ];
    for (const [label, useSaved] of choices) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent =
        key === "settings" && d.review && !useSaved && d.blocked !== "uncertain"
          ? "Review prayer draft"
          : label;
      button.addEventListener("click", () => resolve(key, useSaved));
      element.append(button);
    }
  }
}
async function resolve(key, useSaved) {
  const d = domains[key];
  if (d.operation || storageFault(d)) return;
  // A keyboard change already released before this choice belongs to recovery;
  // subsequent input gets a new version and can never be consumed by this read.
  for (const edit of d.edits.values())
    if (edit.timer) {
      cancelTimer(edit);
      edit.ready = true;
    }
  const before = clone(d.confirmed),
    edits = captureEdits(d),
    review = d.review;
  const operation = {
    kind: "recovery",
    before,
    edits,
    review,
    candidate: merge(before.value, merge(review?.patch || {}, editPatch(releasedEdits(d))))
  };
  d.operation = operation;
  notify(key, "Checking saved state…");
  renderDraftControls();
  try {
    const response = await request(specs[key].read);
    const persisted = matches(key, response.data, operation.candidate, before, review);
    acceptSavedResponse(key, response);
    if (storageFault(d)) {
      d.blocked = "failure";
      recovery(key);
      return;
    }
    if (persisted) {
      finishSaved(key, operation, response, true);
      d.blocked = null;
    } else if (d.blocked === "uncertain") {
      d.blocked = "failure";
      notify(
        key,
        "Saved state checked. Your edits are kept; choose which values to use.",
        "warning"
      );
      recovery(key);
    } else {
      d.blocked = null;
      if (useSaved) {
        acknowledgeEdits(d, edits, true);
        if (d.review === review) d.review = null;
        if (key === "settings" && review) {
          invalidatePreview();
          if (review.version === draftVersion) {
            draftDirty = false;
            helperDraft = false;
            fillDraft(d.confirmed.value);
            status("prayer", "Saved prayer settings restored", "success");
          } else
            status(
              "prayer",
              "Your newer draft is kept. Preview again before confirming.",
              "warning"
            );
        }
      } else if (review) {
        if (d.review === review) d.review = null;
        invalidatePreview();
        status("prayer", "Your draft is kept. Preview again before confirming.", "warning");
      }
    }
    if (!d.blocked)
      for (const group of groupsFor(d, review)) {
        const element = $(group + "-recovery");
        if (element) element.hidden = true;
      }
  } catch (error) {
    d.blocked = error.stale && error.status === 409 && !storageFault(d) ? "conflict" : "uncertain";
    connectionFailure(error);
    notify(
      key,
      d.blocked === "conflict"
        ? "Changed on another client. Your edits are kept; choose which values to use."
        : "Saved state unavailable. Your edits are kept; reconnect to check.",
      "warning"
    );
    recovery(key);
  } finally {
    d.operation = null;
    render();
    await refresh();
    pump();
  }
}
async function refresh() {
  if (loading || writing()) return false;
  loading = true;
  try {
    acceptResponse(await request("/api/status"));
    return true;
  } catch (error) {
    connectionFailure(error);
    render();
    return false;
  } finally {
    loading = false;
    attempted = true;
    if (!connected) render();
  }
}
async function previewDraft() {
  const d = domains.settings;
  if (!draft || !connected || settingsFault() || d.operation || previewOperation) return;
  if (!$("prayer-form").reportValidity()) return;
  readDraft();
  const operation = { version: draftVersion, before: clone(d.confirmed) };
  previewOperation = operation;
  status("prayer", "Preparing timetable preview…");
  renderDraftControls();
  const valid = () =>
    previewOperation === operation &&
    !settingsFault() &&
    operation.version === draftVersion &&
    stable(calculation(operation.before.value)) === stable(calculation(d.confirmed.value));
  try {
    const { data } = await request(
      "/api/preview",
      bodyFor("settings", operation.before, merge(operation.before.value, draft), {
        refreshTimezone: $("refresh-timezone").checked
      })
    );
    if (!valid()) return;
    preview = { ...data, version: operation.version };
    if (firstRun) setupStep = 2;
    renderTimes("preview-times", data, merge(d.confirmed.value, draft), true);
    text(
      "preview-warning",
      data.state === "waiting_for_time"
        ? helperDraft
          ? "Waiting for time. Preview again after the clock is ready before saving this suggestion."
          : "You can finish setup now; announcements will wait for a valid clock."
        : data.state === "invalid_schedule"
          ? "These settings cannot produce a valid schedule. Review location, conventions and offsets."
          : (data.conflicts || []).join(" ")
    );
    $("preview-warning").hidden = !$("preview-warning").textContent;
    text(
      "review-summary",
      "Compare these times with the convention followed by your local mosque."
    );
    status(
      "prayer",
      data.state === "ready" ? "Preview ready · confirm to save" : "Review the preview warning",
      data.state === "ready" ? "" : "warning"
    );
    if (firstRun) {
      renderDraftControls();
      focusStep();
    }
  } catch (error) {
    if (valid())
      status("prayer", error.message + ". Your draft is kept; try preview again.", "error");
  } finally {
    if (previewOperation === operation) {
      previewOperation = undefined;
      renderDraftControls();
    }
  }
}
async function runAction(action) {
  try {
    acceptResponse(
      await request(action.restore ? "/api/cancel-skip" : "/api/skip", {
        expected_revision: domains.settings.confirmed.revision,
        occurrence: action.occurrence
      })
    );
    const changed =
      !sameKey(action.occurrence, snapshot.next) ||
      isSkipped(action.occurrence, snapshot.skip) === !!action.restore;
    status(
      "action",
      changed
        ? "Prayer state changed · review the current prayer"
        : action.name + (action.restore ? " restored" : " will be skipped"),
      changed ? "warning" : "success"
    );
  } catch (error) {
    try {
      acceptResponse(await request("/api/status"));
      const changed = !sameKey(action.occurrence, snapshot.next);
      status(
        "action",
        error.status === 409
          ? "The prayer or settings changed. Check the current prayer before trying again."
          : changed
            ? "Prayer state changed · review the current prayer"
            : "Skip state checked · " +
              (isSkipped(action.occurrence, snapshot.skip) ? "Athan is skipped" : "Athan is on"),
        error.status === 409 || changed ? "warning" : ""
      );
    } catch (readbackError) {
      connectionFailure(readbackError);
      actionUncertain = observedStatus.order < actionOrder;
      status(
        "action",
        actionUncertain
          ? (action.restore ? "Restore" : "Skip") + " unconfirmed · reconnect to check"
          : "Skip state checked · review the current prayer",
        "warning"
      );
    }
  } finally {
    actionBusy = false;
    $("action-feedback").hidden = false;
    render();
  }
}
function stoppedFeedback() {
  stopAwaiting = false;
  status(
    "action",
    "Playback stopped" + (actionUncertain ? " · Skip state unconfirmed; refresh to check." : ""),
    actionUncertain ? "warning" : "success"
  );
  text("settings-playback-feedback", "Playback stopped");
}
async function stopPlayback() {
  if (stopBusy || !connected) return;
  stopBusy = true;
  stopAwaiting = true;
  status("action", "Requesting Stop…");
  text("settings-playback-feedback", "Requesting Stop…");
  $("action-feedback").hidden = false;
  $("settings-playback-feedback").hidden = false;
  render();
  try {
    acceptResponse(await request("/api/stop", {}));
    if (snapshot.playing) {
      stopAwaiting = true;
      status("action", "Stop requested · waiting for playback to stop", "warning");
      text("settings-playback-feedback", "Stop requested · waiting for playback to stop");
    } else stoppedFeedback();
  } catch {
    try {
      acceptResponse(await request("/api/status"));
      const message = snapshot.playing
        ? "Playback is still active. Try Stop again."
        : "Playback stopped · confirmed after readback";
      status("action", message, snapshot.playing ? "warning" : "success");
      text("settings-playback-feedback", message);
    } catch (error) {
      connectionFailure(error);
      const message = "Stop unconfirmed · reconnect to check. Playback may still be active.";
      status("action", message, "warning");
      text("settings-playback-feedback", message);
    }
  } finally {
    stopBusy = false;
    $("action-feedback").hidden = false;
    $("settings-playback-feedback").hidden = false;
    render();
  }
}
function slider(id, key, field) {
  const input = $(id);
  input.addEventListener("input", () => {
    if (!editField(key, field, Number(input.value), false, id)) return;
    renderPreferences();
    notify(key, "Unsaved · release to save", "warning");
  });
  input.addEventListener("change", () => {
    const d = domains[key],
      edit = d?.edits.get(field);
    if (!edit || edit.control !== id) return;
    cancelTimer(edit);
    const submit = () => {
      if (d.edits.get(field) !== edit) return;
      edit.timer = null;
      edit.ready = true;
      queueSave(key);
    };
    if (document.activeElement === input && !input.matches(":active"))
      edit.timer = setTimeout(submit, 350);
    else submit();
  });
}
for (const [id, key, field] of [
  ["volume", "settings", "volume"],
  ["settings-volume", "settings", "volume"],
  ["screen-brightness", "display", "brightness_percent"],
  ["lights-brightness", "lights", "brightness_percent"]
])
  slider(id, key, field);
for (const p of prayers) {
  const label = document.createElement("label"),
    input = document.createElement("input");
  label.className = "check";
  input.type = "checkbox";
  input.id = "enabled-" + p;
  input.disabled = true;
  input.addEventListener("change", () => enqueue("settings", { enabled: { [p]: input.checked } }));
  label.append(input, document.createTextNode(title(p)));
  $("enabled-prayers").append(label);
}
for (const p of events) {
  const label = document.createElement("label"),
    input = document.createElement("input");
  input.type = "number";
  input.id = "offset-" + p;
  input.min = -120;
  input.max = 120;
  input.required = true;
  label.append(document.createTextNode(title(p)), input);
  $("offsets").append(label);
}
document
  .querySelectorAll("[data-view]")
  .forEach((button) => button.addEventListener("click", () => showView(button.dataset.view)));
$("prayer-form").addEventListener("input", markDraft);
$("prayer-form").addEventListener("submit", (e) => {
  e.preventDefault();
  if (!$("confirm").disabled && preview && preview.version === draftVersion)
    enqueue("settings", clone(draft), {
      version: draftVersion,
      refreshTimezone: $("refresh-timezone").checked,
      activate: firstRun
    });
});
$("preview").addEventListener("click", previewDraft);
$("discard").addEventListener("click", discardDraft);
$("setup-next").addEventListener("click", () => {
  if (setupStep === 0) {
    if (!["latitude", "longitude", "timezone"].every((id) => $(id).reportValidity())) return;
    setupStep = 1;
    renderDraftControls();
    focusStep();
  } else previewDraft();
});
$("setup-back").addEventListener("click", () => {
  setupStep = Math.max(0, setupStep - 1);
  invalidatePreview();
  status("prayer", "Draft · preview before saving", "warning");
  renderDraftControls();
  focusStep();
});
$("lights-enabled").addEventListener("change", () =>
  enqueue("lights", { enabled: $("lights-enabled").checked })
);
$("time-format").addEventListener("change", () =>
  enqueue("time_format", { hours: Number($("time-format").value) })
);
$("skip").addEventListener("click", () => {
  if (actionBusy || actionUncertain || !snapshot) return;
  actionBusy = true;
  pendingAction = {
    restore: !!snapshot.skip,
    occurrence: clone(snapshot.skip || snapshot.next),
    name: snapshot.skip ? title(prayers[snapshot.skip.prayer]) : snapshot.next.name
  };
  status("action", "Sending request…");
  $("action-feedback").hidden = false;
  render();
  pump();
});
for (const id of ["stop", "settings-stop"]) $(id).addEventListener("click", stopPlayback);
$("reconnect").addEventListener("click", async () => {
  if (await refresh()) {
    if (actionUncertain) {
      actionUncertain = false;
      status("action", "Skip state checked · review the current prayer");
      render();
      pump();
    }
    for (const key of Object.keys(domains))
      if (domains[key].blocked === "uncertain") await resolve(key, false);
  }
  if (!timezonesReady) await loadTimezones();
});
$("refresh").addEventListener("click", async () => {
  await refresh();
  if (!timezonesReady) await loadTimezones();
});
addEventListener("beforeunload", (e) => {
  if (draftDirty) {
    e.preventDefault();
    e.returnValue = "";
  }
});

function locationMessage(value, error = false) {
  status("prayer", value, error ? "error" : "warning");
}
function incomingLocation() {
  if (!location.hash.startsWith("#v=")) return null;
  const hash = location.hash;
  history.replaceState(null, "", location.pathname + location.search);
  if (hash.length > 512) return { error: true };
  const params = new URLSearchParams(hash.slice(1));
  const allowed = ["v", "latitude", "longitude", "source", "timezone", "accuracy"];
  if (
    [...params.keys()].some((key) => !allowed.includes(key)) ||
    ["v", "latitude", "longitude", "source"].some((key) => params.getAll(key).length !== 1) ||
    ["timezone", "accuracy"].some((key) => params.getAll(key).length > 1) ||
    params.get("v") !== "1"
  )
    return { error: true };
  const latText = params.get("latitude"),
    lonText = params.get("longitude");
  const latitude = Number(latText),
    longitude = Number(lonText),
    source = params.get("source");
  if (
    !latText ||
    !lonText ||
    !Number.isFinite(latitude) ||
    !Number.isFinite(longitude) ||
    latitude < -90 ||
    latitude > 90 ||
    longitude < -180 ||
    longitude > 180 ||
    !["browser", "ip"].includes(source)
  )
    return { error: true };
  const timezone = params.get("timezone") || "";
  if (timezone.length > 64 || /[\x00-\x1f\x7f]/.test(timezone)) return { error: true };
  const accuracyText = params.get("accuracy"),
    accuracy = accuracyText === null ? undefined : Number(accuracyText);
  if (accuracyText !== null && (accuracyText === "" || !Number.isFinite(accuracy) || accuracy < 0))
    return { error: true };
  return { latitude, longitude, timezone, source, accuracy };
}
proposedLocation = incomingLocation();
const helperUrl = new URL("https://openathan.com/location/");
helperUrl.hash = new URLSearchParams({ v: "1", device: location.origin + "/" }).toString();
$("find-location").href = helperUrl.href;
function applyLocationProposal(proposal) {
  if (!proposal) return;
  if (proposal.error) {
    locationMessage("Location link was invalid. Enter your location manually.", true);
    return;
  }
  if (draftDirty && !confirm("Replace your unsaved location edits with the suggested location?")) {
    locationMessage("Your unsaved edits were kept. You can enter the suggested values manually.");
    return;
  }
  $("latitude").value = String(proposal.latitude);
  $("longitude").value = String(proposal.longitude);
  const zoneSupported = !!proposal.timezone && supportedZones.has(proposal.timezone);
  $("timezone").value = zoneSupported ? proposal.timezone : "";
  helperDraft = true;
  showView("settings");
  markDraft();
  const source = proposal.source === "ip" ? "Approximate IP location" : "Browser location";
  const accuracy = Number.isFinite(proposal.accuracy)
    ? proposal.source === "ip"
      ? ` Estimated radius: ${Math.ceil(proposal.accuracy)} km.`
      : ` Reported accuracy: ${Math.ceil(proposal.accuracy)} metres.`
    : "";
  const timezone = zoneSupported
    ? " Check the timezone."
    : " Choose a timezone supported by this device.";
  $("location-feedback").textContent =
    `${source} suggested.${accuracy}${timezone} Preview the timetable before saving.`;
  $("location-feedback").hidden = false;
  locationMessage("Location suggested. Review it and preview the timetable before saving.");
}
function applyPendingLocation() {
  if (!draft || settingsFault() || !timezonesReady || !proposedLocation) return;
  const proposal = proposedLocation;
  proposedLocation = null;
  applyLocationProposal(proposal);
}
addEventListener("hashchange", () => {
  proposedLocation = incomingLocation();
  applyPendingLocation();
});

async function loadTimezones() {
  if (timezonesLoading) return;
  timezonesLoading = true;
  try {
    const { data } = await request("/api/timezones");
    for (const name of data.names) {
      supportedZones.add(name);
      $("zones").append(new Option(name, name));
    }
    timezonesReady = true;
    applyPendingLocation();
  } catch {
    locationMessage(
      "Timezone list unavailable. Your suggestion and manual edits are kept; use Refresh to retry.",
      true
    );
  } finally {
    timezonesLoading = false;
  }
}
let firmwareExpected = "";
try {
  firmwareExpected = sessionStorage.getItem("firmware-expected") || "";
} catch {}
function rememberFirmware(version = "") {
  firmwareExpected = version;
  // This is only a reconnect hint. Device status remains authoritative when
  // browser privacy policy or a storage quota prevents saving it.
  try {
    if (version) sessionStorage.setItem("firmware-expected", version);
    else sessionStorage.removeItem("firmware-expected");
  } catch {}
}
const firmwareMessages = {
  idle: "No update has been requested.",
  checking: "Checking for a stable release…",
  current: "Your firmware is up to date.",
  available: "A firmware update is available.",
  queued: "Update queued. Waiting for a safe time between prayers.",
  downloading: "Downloading the update…",
  verifying: "Verifying the downloaded firmware…",
  restarting: "Restarting with the new firmware. Keep the speaker powered.",
  success: "Firmware updated successfully.",
  rolled_back: "The update could not start successfully. The previous firmware has been restored.",
  failed: "The update could not complete. Check again to retry.",
  storage_fault: "Update storage is unavailable. Restart the device; saved data has been retained."
};
function renderFirmware(state = firmware.data) {
  $("firmware-section").hidden = !state;
  if (!state) return;
  if (firmwareExpected && state.version === firmwareExpected && state.result === "success") {
    rememberFirmware();
  } else if (["rolled_back", "superseded"].includes(state.result)) {
    rememberFirmware();
  }
  text(
    "firmware-version",
    `Installed: ${state.version}${state.queued_version ? ` · Queued: ${state.queued_version}` : state.available ? ` · Available: ${state.available.version}` : ""}`
  );
  const progress =
    state.state === "downloading" && state.total
      ? ` ${Math.floor((100 * state.received) / state.total)}%`
      : "";
  text(
    "firmware-status",
    (state.supported === false
      ? "This speaker needs a maintainer USB update before Wi-Fi installation. "
      : "") +
      (firmwareMessages[state.state] || "Reading update status…") +
      progress +
      (state.error ? ` ${state.error}` : "")
  );
  if (firmwareUncertain)
    text(
      "firmware-status",
      "The response was lost. Waiting to read update status before allowing another action. Keep the speaker powered."
    );
  text(
    "firmware-last-check",
    state.last_check
      ? `Last checked: ${new Date(state.last_check * 1000).toLocaleString(undefined, { hour12: formatHours() === 12, timeZone: "UTC" }) + " UTC"}`
      : "No successful update check yet."
  );
  const working = ["checking", "queued", "downloading", "verifying", "restarting"].includes(
    state.state
  );
  $("firmware-check").disabled =
    firmwareBusy ||
    writing() ||
    !connected ||
    firmwareUncertain ||
    working ||
    state.state === "storage_fault";
  $("firmware-install").hidden = !state.available || working;
  $("firmware-install").disabled =
    firmwareBusy ||
    writing() ||
    !connected ||
    firmwareUncertain ||
    state.state === "storage_fault" ||
    state.supported === false;
  $("firmware-cancel").hidden = !["queued", "downloading", "verifying"].includes(state.state);
  $("firmware-cancel").disabled = firmwareBusy || writing() || !connected || firmwareUncertain;
  $("firmware-notes").hidden = !state.available;
  if (state.available)
    $("firmware-notes").href =
      `https://github.com/OpenAthan-Project/openathan/releases/tag/${encodeURIComponent(state.available.version)}`;
}
async function firmwareAction(action) {
  if (firmwareBusy || firmwareUncertain || !firmware.data || writing()) return;
  firmwareBusy = true;
  renderFirmware(firmware.data);
  const version = firmware.data.available?.version;
  try {
    if (action === "install") rememberFirmware(version);
    acceptResponse(
      await request(`/api/firmware/${action}`, action === "install" ? { version } : {}),
      "firmware"
    );
    if (action === "cancel") rememberFirmware();
  } catch (error) {
    firmwareUncertain = !error.status && firmware.order < firmwareActionOrder;
    $("firmware-status").textContent = `${error.message}. Reading update status before retrying.`;
    try {
      acceptResponse(await request("/api/firmware"), "firmware");
    } catch {
      $("firmware-status").textContent =
        "The speaker is unavailable. Keep it powered; status will reconnect automatically.";
    }
  } finally {
    firmwareBusy = false;
    if (firmware.data) renderFirmware(firmware.data);
    pump();
  }
}
for (const action of ["check", "install", "cancel"])
  $("firmware-" + action).addEventListener("click", () => firmwareAction(action));
setInterval(async () => {
  if (document.hidden || firmwareBusy || !firmware.data) return;
  try {
    acceptResponse(await request("/api/firmware"), "firmware");
  } catch (error) {
    if (
      error.order >= firmware.order &&
      (firmwareExpected || ["downloading", "verifying", "restarting"].includes(firmware.data.state))
    )
      text("firmware-status", "Waiting for the speaker to reconnect. Keep it powered.");
  }
}, 3000);

render();
(async () => {
  await refresh();
  await loadTimezones();
})();
setInterval(() => {
  if (!document.hidden) refresh();
}, 5000);

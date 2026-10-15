"use strict";
const $ = (id) => document.getElementById(id);
const bytes = (value) => new TextEncoder().encode(value).length;
let observed, manual = false, writing = false, submitting = false, uncertain = false, scanTimer, statusTimer, statusRevision = 0;
let scanning = false, scanPolls = 0, scanRevision = 0, statusPolls = 0, selectedSsid = "";
function feedback(id, message = "", error = false) {
  $(id).textContent = message;
  $(id).classList.toggle("error", error);
}
function step(name, focus = true) {
  for (const item of ["password", "network", "connect", "unavailable"]) {
    $(item + "-step").hidden = item !== name;
    const progress = $("progress-" + item);
    if (progress) {
      if (item === name) progress.setAttribute("aria-current", "step");
      else progress.removeAttribute("aria-current");
    }
  }
  if (focus) $(name + "-heading").focus();
}
async function api(path, body) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 10000);
  try {
    const response = await fetch("/setup/" + path, {
      method: body === undefined ? "GET" : "POST", credentials: "omit", cache: "no-store", redirect: "error",
      headers: body === undefined ? { Accept: "application/json" } : { Accept: "application/json", "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body), signal: controller.signal
    });
    const data = await response.json();
    if (!response.ok) {
      const error = new Error(typeof data.error === "string" ? data.error.slice(0, 240) : "The speaker could not complete this request.");
      error.status = response.status; throw error;
    }
    return { status: response.status, data };
  } finally { clearTimeout(timer); }
}
function speakerAddress(value) {
  try {
    const url = new URL(value);
    if (url.protocol !== "http:" || !url.hostname.endsWith(".local") || url.username || url.password ||
        url.port || url.pathname !== "/" || url.search || url.hash) return false;
    $("speaker-link").href = url.href;
    $("speaker-link").textContent = url.href;
    return true;
  } catch { return false; }
}
function applyStatus(data) {
  if (!data || !["ready", "connecting", "failed", "storage_fault"].includes(data.state) ||
      typeof data.password_set !== "boolean" || typeof data.setup_ssid !== "string" || !speakerAddress(data.speaker_url))
    throw new Error("Speaker status is unavailable.");
  observed = data;
  $("connection").textContent = "Connected to " + data.setup_ssid;
  $("recovery-ssid").textContent = data.setup_ssid;
  $("confirm-field").hidden = data.password_set;
  $("confirm-password").required = !data.password_set;
  $("speaker-password").autocomplete = data.password_set ? "current-password" : "new-password";
  $("password-heading").textContent = data.password_set ? "Enter your speaker password" : "Protect your speaker";
  $("password-description").textContent = data.password_set
    ? "Use the speaker password already chosen during setup. Your Wi-Fi password is entered separately."
    : "Choose a password for the speaker’s settings page. It is separate from your Wi-Fi password.";
  if (data.state === "storage_fault") {
    writing = false; uncertain = false; clearTimeout(statusTimer); step("unavailable");
    feedback("page-feedback", "The speaker could not read or save its setup information.", true); return;
  }
  if (data.state === "connecting") {
    writing = true; handoff(); feedback("connect-feedback", "The speaker is trying to join your home Wi-Fi. Follow the steps above if this window disconnects.");
    scheduleStatus(); return;
  }
  writing = false; uncertain = false;
  if (data.state === "failed") {
    step("password");
    feedback("page-feedback", typeof data.error === "string" && data.error ? data.error.slice(0, 240) + " Enter your passwords to try again." : "The speaker could not join Wi-Fi. Check your passwords and try again.", true);
  } else { feedback("page-feedback"); step("password", false); }
}
async function readStatus() {
  const revision = ++statusRevision;
  $("retry-status").hidden = true; $("check-connection").disabled = true;
  try { const { data } = await api("status"); if (revision === statusRevision) applyStatus(data); }
  catch {
    if (revision !== statusRevision) return;
    if (writing || uncertain) lostConnection();
    else { feedback("page-feedback", "Could not reach the speaker. Stay on its setup Wi-Fi and check again.", true); $("retry-status").hidden = false; }
  } finally { $("check-connection").disabled = submitting; }
}
function handoff() {
  cancelScan();
  $("home-name").textContent = selectedSsid || "your home Wi-Fi";
  $("connect-actions").hidden = writing || uncertain;
  $("connection-recovery").hidden = !(writing || uncertain);
  $("check-connection").disabled = submitting;
  $("before-connect").textContent = writing || uncertain
    ? "The speaker may have left this setup network. Use your home Wi-Fi to open its settings page."
    : "The setup network will close while the speaker tries your home Wi-Fi. Keep these steps handy before continuing.";
  step("connect", $("connect-step").hidden);
}
function lostConnection() {
  clearTimeout(statusTimer); writing = false; uncertain = true; handoff();
  $("connection").textContent = "Setup connection changed";
  feedback("connect-feedback", "Connection changed. We cannot confirm whether the speaker joined Wi-Fi. Try the speaker page on your home Wi-Fi, or rejoin the setup network when it is available again.", true);
}
function scheduleStatus() {
  clearTimeout(statusTimer);
  if (++statusPolls > 30) { feedback("connect-feedback", "The speaker is still connecting. Check your home Wi-Fi, or check setup status again."); return; }
  statusTimer = setTimeout(readStatus, 1500);
}
function passwordValid() {
  const password = $("speaker-password");
  password.setCustomValidity(/^[\x20-\x7e]{12,128}$/.test(password.value) ? "" : "Use 12–128 English letters, numbers, spaces or symbols.");
  const confirm = $("confirm-password");
  confirm.setCustomValidity(observed?.password_set || confirm.value === password.value ? "" : "The speaker passwords do not match.");
  return $("password-form").reportValidity();
}
function ssid() { return manual ? $("manual-ssid").value : $("network").value; }
function networkValid() {
  const field = manual ? $("manual-ssid") : $("network");
  const name = ssid();
  field.setCustomValidity(name && bytes(name) <= 32 && !name.includes("\0") ? "" : "Enter a Wi-Fi network name of up to 32 bytes.");
  const password = $("wifi-password"), length = bytes(password.value);
  password.setCustomValidity(!password.value.includes("\0") && ((length >= 8 && length <= 63) || /^[0-9a-fA-F]{64}$/.test(password.value))
    ? "" : "This Wi-Fi password is too short or too long for the speaker.");
  return $("network-form").reportValidity();
}
function cancelScan() { ++scanRevision; clearTimeout(scanTimer); scanning = false; $("scan").disabled = false; }
function networks(data, revision) {
  if (!data || typeof data.scanning !== "boolean" || !Array.isArray(data.networks)) throw new Error("Invalid scan result");
  const select = $("network"), previous = select.value;
  select.replaceChildren(new Option("Choose a network", ""));
  const seen = new Set();
  for (const network of data.networks) {
    if (typeof network.ssid !== "string" || typeof network.secured !== "boolean" || !network.ssid || bytes(network.ssid) > 32 || network.ssid.includes("\0") || seen.has(network.ssid)) continue;
    seen.add(network.ssid);
    const option = new Option(network.ssid + (network.secured ? "" : " — open network unsupported"), network.ssid);
    option.disabled = !network.secured; select.add(option);
  }
  if (Array.from(select.options).some((option) => option.value === previous && !option.disabled)) select.value = previous;
  scanning = data.scanning;
  $("scan").disabled = scanning;
  feedback("scan-feedback", scanning ? "Looking for Wi-Fi networks…" : seen.size ? "Open networks are not supported. You can also enter a hidden network’s name." : "No networks found. Scan again or enter the network name.");
  if (scanning && ++scanPolls < 20) scanTimer = setTimeout(() => readNetworks(revision), 1000);
  else if (scanning) { scanning = false; $("scan").disabled = false; feedback("scan-feedback", "The scan is taking longer than expected. Scan again or enter the network name.", true); }
}
async function readNetworks(revision = scanRevision) {
  if (writing || uncertain || revision !== scanRevision) return;
  try { const { data } = await api("networks"); if (revision === scanRevision) networks(data, revision); }
  catch { if (revision === scanRevision) { scanning = false; $("scan").disabled = false; feedback("scan-feedback", "Could not read Wi-Fi networks. Scan again or enter the network name.", true); } }
}
async function scan() {
  if (scanning || writing || uncertain) return;
  const revision = ++scanRevision;
  clearTimeout(scanTimer); scanning = true; scanPolls = 0; $("scan").disabled = true;
  feedback("scan-feedback", "Looking for Wi-Fi networks…");
  try { await api("scan", {}); if (revision === scanRevision) await readNetworks(revision); }
  catch { if (revision === scanRevision) { scanning = false; $("scan").disabled = false; feedback("scan-feedback", "Could not scan Wi-Fi. Try again or enter the network name.", true); } }
}
$("password-form").addEventListener("submit", (event) => {
  event.preventDefault(); if (!passwordValid()) return;
  feedback("page-feedback"); step("network"); scan();
});
for (const id of ["speaker-password", "confirm-password", "wifi-password", "manual-ssid", "network"])
  $(id).addEventListener("input", () => {
    $(id).setCustomValidity("");
    if (id === "speaker-password" || id === "confirm-password") {
      $("speaker-password").setCustomValidity(""); $("confirm-password").setCustomValidity("");
    }
  });
$("show-speaker-password").addEventListener("change", (event) => {
  for (const id of ["speaker-password", "confirm-password"]) $(id).type = event.target.checked ? "text" : "password";
});
$("show-wifi-password").addEventListener("change", (event) => { $("wifi-password").type = event.target.checked ? "text" : "password"; });
$("manual-toggle").addEventListener("click", () => {
  manual = !manual; $("manual-field").hidden = !manual; $("manual-ssid").disabled = !manual; $("manual-ssid").required = manual;
  $("network-picker").hidden = manual; $("network").disabled = manual; $("network").required = !manual;
  $("manual-toggle").textContent = manual ? "Choose from scanned networks" : "Enter network name instead";
  $(manual ? "manual-ssid" : "network").focus();
});
$("network-form").addEventListener("submit", (event) => {
  event.preventDefault(); if (!networkValid()) return;
  selectedSsid = ssid(); feedback("connect-feedback"); handoff();
});
$("network-back").addEventListener("click", () => { cancelScan(); step("password"); });
$("connect-back").addEventListener("click", () => step("network"));
$("scan").addEventListener("click", scan);
$("connect").addEventListener("click", async () => {
  if (writing || uncertain || !passwordValid() || !networkValid()) return;
  writing = submitting = true; ++statusRevision; statusPolls = 0; handoff(); feedback("connect-feedback", "Sending connection details to your speaker…");
  const payload = { password: $("speaker-password").value, ssid: selectedSsid, wifi_password: $("wifi-password").value };
  const pending = api("connect", payload);
  payload.password = payload.wifi_password = "";
  for (const id of ["speaker-password", "confirm-password", "wifi-password"]) $(id).value = "";
  try {
    const { status, data } = await pending;
    submitting = false; $("check-connection").disabled = false;
    if (status !== 202 || data.state !== "connecting" || !speakerAddress(data.speaker_url)) throw new Error("Unconfirmed connection request");
    $("connection").textContent = "Connecting speaker to home Wi-Fi";
    feedback("connect-feedback", "The speaker is trying to join your home Wi-Fi. Follow the steps above if this window disconnects.");
    scheduleStatus();
  } catch (error) {
    submitting = false; $("check-connection").disabled = false;
    if (error.status === 503) { writing = false; step("unavailable"); feedback("page-feedback", error.message, true); }
    else if ([400, 403, 409, 429].includes(error.status)) {
      writing = false; step("password"); feedback("page-feedback", error.message + " Enter your passwords again.", true);
      try { applyStatus((await api("status")).data); feedback("page-feedback", error.message + " Enter your passwords again.", true); } catch { /* Keep the explicit rejection; no write is retried. */ }
    } else lostConnection();
  }
});
$("retry-status").addEventListener("click", readStatus);
$("check-connection").addEventListener("click", readStatus);
$("setup-address").textContent = location.origin + "/setup/";
readStatus();

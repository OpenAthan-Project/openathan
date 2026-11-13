// Run the production automatic policy with the same deterministic I/O as OTA.
#define main existing_ota_tests
#include "upgrade_runtime_tests.cpp"
#undef main

JsonDocument snapshot(Upgrade &u) { JsonDocument doc; u.snapshot(doc.to<JsonObject>()); return doc; }
int automatic(Upgrade &u, bool enabled, uint32_t revision) {
  JsonDocument input; input["schema"] = 1; input["enabled"] = enabled; input["expected_revision"] = revision;
  std::string error; return u.action("automatic", input.as<JsonObject>(), error);
}
void automatic_queue(Upgrade &u) {
  u.loop(true); run_worker(); // Startup discovery, no owner action.
  assert(state(u) == "available"); u.loop(true);
  assert(state(u) == "queued" && snapshot(u)["queue_source"] == "automatic");
  assert(!u.streaming_blocked());
}
std::string identity() {
  JsonDocument envelope, payload;
  assert(!deserializeJson(envelope, descriptor_response));
  assert(!deserializeJson(payload, envelope["payload"].as<std::string>()));
  return payload["version"].as<std::string>() + ":" + payload["sha256"].as<std::string>();
}
void saved_policy(bool enabled, const std::string &request, const std::string &skipped = "") {
  JsonDocument doc; doc["schema"] = 1; doc["revision"] = 1; doc["enabled"] = enabled;
  doc["skipped"] = skipped; doc["request"] = request; serializeJson(doc, saved_automatic);
}
void retain_request(const char *expected = "") {
  JsonDocument doc; doc["schema"] = 1; doc["queue"] = descriptor_response; doc["expected"] = expected;
  serializeJson(doc, saved_record);
}
void turn_off_during_read() { assert(automatic(*callback_update, false, 0) == 200); }

int main() {
  for (const char *invalid : {R"({"schema":1,"enabled":1,"expected_revision":0})",
      R"({"schema":2,"enabled":false,"expected_revision":0})",
      R"({"schema":1,"enabled":false,"expected_revision":-1})",
      R"({"schema":1,"enabled":false,"expected_revision":0,"extra":true})"}) {
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    JsonDocument input; assert(!deserializeJson(input, invalid)); std::string error;
    assert(u.action("automatic", input.as<JsonObject>(), error) == 400 && nvs_commits == 0);
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    assert(snapshot(u)["automatic_updates"]["enabled"] == true && nvs_commits == 0);
    assert(automatic(u, false, 0) == 200);
    assert(snapshot(u)["automatic_updates"]["revision"] == 1);
    assert(automatic(u, true, 0) == 409);
    Upgrade reboot; begin(reboot, a);
    assert(snapshot(reboot)["automatic_updates"]["enabled"] == false);
    assert(automatic(reboot, false, 1) == 200 && nvs_commits == 1);
    assert(action(reboot, "automatic") == 400);
  }
  if (!newer_release("v0.3.0", OPENATHAN_FIRMWARE_VERSION)) {
    reset(); saved_policy(true, identity()); retain_request("v0.3.0"); running_state = ESP_OTA_IMG_PENDING_VERIFY;
    Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    expect_record("", ""); expect_result(u, "success");
    assert(confirmations == 1 && snapshot(u)["automatic_updates"]["enabled"] == true);
    std::cout << "Automatic request confirms successfully in the installed release\n"; return 0;
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    u.loop(true); assert(u.streaming_blocked()); run_worker(); u.loop(true);
    assert(boot_selections == 1 && esphome::App.reboots == 1);
    expect_record(descriptor_response, "v0.3.0");
    assert(automatic(u, false, 0) == 409); // Selection is already complete.
  }
  for (unsigned gate = 0; gate < 7; ++gate) {
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); offer(u);
    if (gate == 0) a.sample.playing = true;
    if (gate == 1) u.set_background_busy(true);
    if (gate == 2) a.clock.valid = false;
    if (gate == 3) a.active = false;
    if (gate == 4) a.sample.next->utc = a.clock.utc + 899;
    if (gate == 5) a.sample.automatic_ready = false;
    if (gate == 6) a.health = false;
    u.loop(true); assert(saved_record.empty() && erases == 0 && !pending_worker);
    a.sample.playing = false; u.set_background_busy(false); a.clock.valid = a.active = a.health = true;
    a.sample.automatic_ready = true; a.sample.next->utc = a.clock.utc + 900;
    u.loop(true); assert(state(u) == "queued" && !u.streaming_blocked());
    a.sample.playing = true; u.loop(true); assert(!pending_worker && !u.streaming_blocked());
    a.sample.playing = false; u.loop(true); run_worker();
    a.sample.next->utc = a.clock.utc + 899; u.loop(true); assert(boot_selections == 0);
    a.sample.next->utc = a.clock.utc + 900; u.loop(true); assert(boot_selections == 1);
  }
  for (bool manual : {false, true}) {
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    if (manual) queue(u); else automatic_queue(u);
    assert(automatic(u, false, 0) == 200);
    assert(state(u) == (manual ? "queued" : "idle"));
    Upgrade reboot; begin(reboot, a); reboot.loop(true);
    if (manual) { assert(pending_worker); run_worker(); reboot.loop(true); assert(boot_selections == 1); }
    else { run_worker(); reboot.loop(true); assert(erases == 0 && boot_selections == 0); }
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    u.loop(true); callback_update = &u; on_read = turn_off_during_read; run_worker();
    assert(state(u) == "idle" && writes == 0 && aborts == 1 && boot_selections == 0);
  }
  // Cuts at either preference/cancellation boundary cannot revive a request.
  for (bool manual : {false, true}) for (bool disable : {false, true}) for (unsigned commit : {1u, 2u}) {
    if (manual && disable) continue; // Opt-out deliberately preserves manual requests.
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    if (manual) queue(u); else automatic_queue(u);
    cut_commit_number = nvs_commits + commit;
    try { if (disable) automatic(u, false, 0); else action(u, "cancel"); assert(false); }
    catch (const PowerCut &) {}
    cut_commit_number = 0;
    Upgrade reboot; begin(reboot, a); assert(state(reboot) == "idle");
    offer(reboot); reboot.loop(true); assert(state(reboot) == "available" && erases == 0);
    Upgrade again; begin(again, a); offer(again); again.loop(true); assert(state(again) == "available");
    if (!disable) { assert(snapshot(again)["automatic_updates"]["release_skipped"] == true);
      assert(action(again, "install", "v0.3.0") == 200 && snapshot(again)["queue_source"] == "manual"); }
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    u.loop(true); run_worker(); on_boot_selection = power_cut;
    try { u.loop(true); assert(false); } catch (const PowerCut &) {}
    on_boot_selection = nullptr; boot_partition = &running; inactive_state = ESP_OTA_IMG_ABORTED;
    Upgrade rollback; begin(rollback, a); expect_result(rollback, "rolled_back");
    offer(rollback); rollback.loop(true); assert(state(rollback) == "available" && erases == 1);
    Upgrade reboot; begin(reboot, a); offer(reboot); reboot.loop(true); assert(state(reboot) == "available" && erases == 1);
  }
  for (bool manual : {false, true}) for (unsigned failure = 0; failure < 3; ++failure) {
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    if (manual) queue(u); else automatic_queue(u);
    if (failure == 0) application_response[20] = 'b';
    if (failure == 1) image_valid = false;
    if (failure == 2) inactive_version = "v0.2.1";
    u.loop(true); run_worker(); assert(state(u) == "failed" && live_ota_handles == 0 && boot_selections == 0);
    expect_record("", ""); assert(snapshot(u)["automatic_updates"]["skipped_version"] == "v0.3.0");
    const auto count = erases; Upgrade reboot; begin(reboot, a); offer(reboot); reboot.loop(true);
    assert(state(reboot) == "available" && erases == count && boot_selections == 0);
    assert(snapshot(reboot)["automatic_updates"]["release_skipped"] == true);
  }
  for (bool manual : {false, true}) for (bool restart : {false, true}) {
    reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    if (manual) queue(u); else automatic_queue(u);
    const auto request = saved_record, policy = saved_automatic;
    description_query_fails = true; u.loop(true); run_worker();
    if (state(u) != "queued") {
      std::cerr << "Description read failure for " << (manual ? "manual" : "automatic")
                << " request: state=" << state(u)
                << ", skipped_version=" << snapshot(u)["automatic_updates"]["skipped_version"].as<std::string>()
                << ", request_retained=" << (saved_record == request) << "\n";
    }
    assert(state(u) == "queued" && live_ota_handles == 0 && boot_selections == 0 && esphome::App.reboots == 0);
    assert(saved_record == request && saved_automatic == policy);
    assert(snapshot(u)["automatic_updates"]["skipped_version"] == "");
    assert(snapshot(u)["queue_source"] == (manual ? "manual" : "automatic"));
    const auto attempts = task_attempts;
    now_us += 299999000; u.loop(true);
    assert(task_attempts == attempts && !pending_worker && boot_selections == 0);
    description_query_fails = false;
    Upgrade reboot;
    if (restart) { u.shutdown(); now_us = 0; begin(reboot, a); }
    else now_us += 1000;
    Upgrade &retry = restart ? reboot : u;
    assert(state(retry) == "queued" && snapshot(retry)["queue_source"] == (manual ? "manual" : "automatic"));
    assert(saved_record == request && saved_automatic == policy);
    retry.loop(true); run_worker();
    assert(live_ota_handles == 0 && boot_selections == 0);
    retry.loop(true);
    assert(boot_selections == 1 && esphome::App.reboots == 1 && erases == 2);
    expect_record(descriptor_response, "v0.3.0");
    assert(snapshot(retry)["automatic_updates"]["skipped_version"] == "");
  }
  { reset(); saved_policy(true, "", "v0.2.1"); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
    automatic_queue(u); assert(snapshot(u)["automatic_updates"]["release_skipped"] == false); }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    Upgrade reboot; begin(reboot, a); assert(snapshot(reboot)["queue_source"] == "automatic");
    u.shutdown(); // Only the simulated reboot instance remains active.
    assert(automatic(reboot, false, 0) == 200 && state(reboot) == "idle");
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    a.active = false; Upgrade reboot; begin(reboot, a); reboot.loop(true);
    assert(!pending_worker && erases == 0 && !reboot.streaming_blocked());
    a.active = true; reboot.loop(true); run_worker();
    a.active = false; reboot.loop(true); assert(boot_selections == 0);
    a.active = true; reboot.loop(true); assert(boot_selections == 1);
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); automatic_queue(u);
    transfer_fail = true; u.loop(true); run_worker(); assert(state(u) == "queued");
    assert(snapshot(u)["automatic_updates"]["skipped_version"] == "");
    transfer_fail = false; const auto attempts = task_attempts;
    now_us += 299999000; u.loop(true); assert(task_attempts == attempts);
    now_us += 1000; u.loop(true); run_worker(); u.loop(true); assert(boot_selections == 1);
  }
  { reset(); boot_readable = false; Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); offer(u); u.loop(true);
    assert(saved_record.empty() && erases == 0 && snapshot(u)["supported"] == false);
  }
  { reset(); Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); offer(u); commit_ok = false;
    u.loop(true); assert(saved_record.empty() && erases == 0 && !pending_worker);
    assert(snapshot(u)["automatic_updates"]["application"] == "storage_fault");
  }
  { reset(); saved_automatic = "corrupt"; retain_request(); running_state = ESP_OTA_IMG_PENDING_VERIFY;
    Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a); u.loop(true);
    assert(confirmations == 1 && !pending_worker && erases == 0 && snapshot(u)["queue_source"] == "unknown");
    assert(snapshot(u)["automatic_updates"]["enabled"].isNull() && automatic(u, false, 0) == 503);
    const auto request = saved_record;
    assert(action(u, "cancel") == 503 && saved_record == request);
  }
  std::cout << "Default-on automatic updates, opt-out, safe gaps, interruption and rejection suppression passed\n";
}

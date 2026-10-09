#define main production_fixture_main
#include "upgrade_runtime_tests.cpp"
#undef main
int main() {
  reset();
  // An unrelated old test journal must neither block confirmation nor be rewritten.
  saved_record = "old isolated journal";
  running_state = ESP_OTA_IMG_PENDING_VERIFY;
  Upgrade u; esphome::openathan_component::OpenAthan a; begin(u, a);
  now_us = 60000000; u.loop(true);
  assert(confirmations == 1 && rollbacks == 0 && u.usb_info()[5] == "confirmed");
  assert(u.usb_info()[3] == "unsupported");
  JsonDocument doc; u.snapshot(doc.to<JsonObject>());
  assert(doc["hardware"].as<std::string>() == "waveshare-esp32-s3-touch-lcd-1_85c-box-v2");
  assert(!doc["updates_enabled"].as<bool>() && !doc["supported"].as<bool>());
  for (const auto *name : {"check", "install", "cancel"}) assert(action(u, name) == 503);
  std::vector<std::string> reply;
  for (auto command : {::openathan::usb_upgrade::BEGIN, ::openathan::usb_upgrade::VERIFY,
                       ::openathan::usb_upgrade::ABORT, ::openathan::usb_upgrade::FINISH})
    assert(u.usb_command(command, {"512"}, reply) == 2);
  assert(!u.usb_chunk({}));
  for (int day = 1; day <= 3; ++day) { now_us = int64_t(day)*86400*1000000; u.loop(true); }
  assert(task_attempts == 0 && !pending_worker && erases == 0 && writes == 0 && boot_selections == 0);
  assert(nvs_commits == 0 && saved_record == "old isolated journal");
  reset(); running_state = ESP_OTA_IMG_PENDING_VERIFY;
  Upgrade broken; a.failed = true; begin(broken, a);
  now_us = 91000000; broken.loop(true); assert(rollbacks == 1 && confirmations == 0);
  std::cout << "Disabled updates preserve health confirmation and reject every update transport\n";
}

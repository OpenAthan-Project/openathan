// Share deterministic fixtures; execute separate qualification scenarios.
#define main production_fixture_main
#include "upgrade_runtime_tests.cpp"
#undef main
int control(Upgrade &u,const char *command,const char *phase=nullptr){
  JsonDocument doc;doc["command"]=command;if(phase)doc["phase"]=phase;
  std::string error;return u.action("qualification",doc.as<JsonObject>(),error);
}
int main(){
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;a.health=false;
    begin(u,a);assert(confirmations==0&&action(u,"check")==200);run_worker();
    assert(state(u)=="available"&&action(u,"install","v0.3.0")==503);
    assert(action(u,"cancel")==503&&control(u,"arm","before_boot_selection")==503);
    assert(erases==0&&writes==0&&boot_selections==0&&saved_record.empty());
    auto stack_free=[&](){JsonDocument doc;u.snapshot(doc.to<JsonObject>());return doc["qualification"]["worker_stack_min_free"].as<unsigned>();};
    assert(stack_free()==4096);worker_stack_free_bytes=2048;
    assert(action(u,"check")==200);run_worker();assert(stack_free()==2048);
    worker_stack_free_bytes=4096;assert(action(u,"check")==200);run_worker();assert(stack_free()==2048);
  }
  if (OPENATHAN_QUALIFICATION_STARTUP_FAILURE) {
    reset();running_state=ESP_OTA_IMG_PENDING_VERIFY;
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(confirmations==0);now_us=90000000;u.loop(false);
    assert(confirmations==0&&rollbacks==1&&boot_selections==0);
    std::cout<<"Qualification startup failure refuses confirmation and rolls back\n";return 0;
  }
  for(const char *phase:{"before_boot_selection","after_boot_selection"}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(control(u,"arm",phase)==200);queue(u);
    assert(last_url==std::string(OPENATHAN_QUALIFICATION_ORIGIN)+"/releases/latest/download/upgrade.json");
    assert(last_ca==OPENATHAN_QUALIFICATION_CA&&!last_bundle);
    u.loop(true);run_worker();u.loop(true);
    assert(esphome::App.reboots==0 && boot_selections==(std::string(phase)=="after_boot_selection"?1:0));
    expect_record(descriptor_response,"v0.3.0");
    const auto commits=nvs_commits;
    for(unsigned i=0;i<10;++i)u.loop(true);
    assert(nvs_commits==commits);
    assert(boot_selections==(std::string(phase)=="after_boot_selection"?1:0));
    assert(control(u,"release")==200);a.sample.playing=true;u.loop(true);assert(esphome::App.reboots==0);
    a.sample.playing=false;u.loop(true);assert(boot_selections==1&&esphome::App.reboots==1);
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);control(u,"arm","before_boot_selection");
    queue(u);u.loop(true);run_worker();u.loop(true);assert(action(u,"cancel")==200);
    u.loop(true);assert(boot_selections==0&&esphome::App.reboots==0);expect_record("","");}
  {reset();metadata_erased=true;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(boot_selections==1&&esphome::App.reboots==1);running_state=ESP_OTA_IMG_PENDING_VERIFY;
    now_us=0;Upgrade reboot;begin(reboot,a);assert(confirmations==1);}
  {reset();metadata_erased=true;metadata_corrupt=true;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(boot_selections==0&&state(u)=="storage_fault"&&action(u,"check")==503&&task_attempts==0);
    metadata_erased=metadata_corrupt=false;}
  {reset();running_state=ESP_OTA_IMG_UNDEFINED;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(confirmations==1&&running_state==ESP_OTA_IMG_VALID);}
  std::cout<<"Qualification holds, TLS profile and baseline confirmation passed\n";
}

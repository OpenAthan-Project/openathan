// Exercise the production updater with deterministic transport/flash/task/NVS
// adapters. Signature success is injected here; cryptography is tested separately.
#include "upgrade.h"
#include "esphome/core/application.h"
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>
#include <freertos/task.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <cassert>
#include <iostream>
using namespace esphome::openathan_device;
int64_t now_us{};
int64_t esp_timer_get_time(){return now_us;}
std::string saved_record, staged_record;
unsigned nvs_commits{};
bool commit_ok=true;
bool cut_after_commit=false;
struct PowerCut {};
int nvs_open(const char *,int,nvs_handle_t *h){*h=1;return 0;}
int nvs_get_blob(nvs_handle_t,const char *,void *out,size_t *size){
  if(saved_record.empty())return ESP_ERR_NVS_NOT_FOUND;
  if(out)memcpy(out,saved_record.data(),saved_record.size());
  *size=saved_record.size();return 0;
}
int nvs_set_blob(nvs_handle_t,const char *,const void *data,size_t size){staged_record.assign(static_cast<const char *>(data),size);return 0;}
int nvs_commit(nvs_handle_t){++nvs_commits;if(!commit_ok)return -1;saved_record=staged_record;if(cut_after_commit)throw PowerCut{};return 0;}
void nvs_close(nvs_handle_t){}
int nvs_erase_all(nvs_handle_t){assert(false);return -1;}
std::string state(Upgrade &update){JsonDocument doc;update.snapshot(doc.to<JsonObject>());return doc["state"].as<std::string>();}
int action(Upgrade &update,const char *name,const char *version=nullptr){JsonDocument doc;auto root=doc.to<JsonObject>();if(version)root["version"]=version;std::string error;return update.action(name,root,error);}
void reset(){assert(live_read_buffers==0);read_buffer_allocation_fails=false;read_buffer_allocation_attempts=0;now_us=0;saved_record.clear();staged_record.clear();commit_ok=true;cut_after_commit=false;signature_valid=true;boot_readable=true;transfer_fail=false;on_open=on_read=nullptr;pending_worker=nullptr;task_available=true;task_attempts=0;boot_selections=aborts=rollbacks=confirmations=erases=writes=live_ota_handles=0;begin_allocation_fails=begin_erase_fails=false;image_valid=true;running_state=ESP_OTA_IMG_VALID;inactive_state=ESP_OTA_IMG_UNDEFINED;boot_partition=&running;on_boot_selection=nullptr;inactive_version="v0.3.0";esphome::App.reboots=0;
  application_response=std::string(512,'a');uint8_t bytes[32];mbedtls_sha256(reinterpret_cast<const uint8_t *>(application_response.data()),application_response.size(),bytes,0);
  std::string hash;for(auto b:bytes){hash+="0123456789abcdef"[b>>4];hash+="0123456789abcdef"[b&15];}
  JsonDocument payload;payload["schema"]=1;payload["version"]="v0.3.0";payload["commit"]=std::string(40,'a');payload["hardware"]="atoms3r-c126-pyramid-a167";payload["layout"]="dual-2m-audio-3_5m-v1";payload["storageFormat"]=1;payload["audioFormat"]=1;payload["rollback"]=true;payload["bytes"]=512;payload["sha256"]=hash;
  std::string text;serializeJson(payload,text);JsonDocument envelope;envelope["payload"]=text;envelope["signature"]=std::string(128,'0');descriptor_response.clear();serializeJson(envelope,descriptor_response);
}
void begin(Upgrade &update,esphome::openathan_component::OpenAthan &athan){update.begin(&athan,true);now_us+=30000000;update.loop(false);}
void offer(Upgrade &update){assert(action(update,"check")==200);run_worker();assert(state(update)=="available");}
void queue(Upgrade &update){offer(update);assert(action(update,"install","v0.3.0")==200);assert(state(update)=="queued");assert(!saved_record.empty());}
Upgrade *callback_update;
esphome::openathan_component::OpenAthan *callback_athan;
void cancel_download(){assert(action(*callback_update,"cancel")==200);}
void prayer_starts(){callback_athan->sample.playing=true;callback_update->loop(true);}
void power_cut(){throw PowerCut{};}
void expect_record(const std::string &queue,const char *expected) {
  JsonDocument doc;assert(!deserializeJson(doc,saved_record));
  assert(doc["queue"].as<std::string>()==queue && doc["expected"].as<std::string>()==expected);
}
void expect_result(Upgrade &update,const char *result) {
  JsonDocument doc;update.snapshot(doc.to<JsonObject>());assert(doc["result"].as<std::string>()==result);
}
int main(){
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    JsonDocument doc;u.snapshot(doc.to<JsonObject>());assert(doc["qualification"].isUnbound());
    assert(action(u,"qualification")==400&&erases==0&&writes==0&&boot_selections==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;a.health=false;
    begin(u,a);assert(action(u,"check")==503&&task_attempts==0);
    assert(erases==0&&writes==0&&boot_selections==0&&saved_record.empty());}
  if(std::string(OPENATHAN_FIRMWARE_VERSION)=="v0.3.0") {
    reset();JsonDocument doc;doc["schema"]=1;doc["queue"]=descriptor_response;doc["expected"]="v0.3.0";
    serializeJson(doc,saved_record);running_state=ESP_OTA_IMG_PENDING_VERIFY;
    Upgrade updated;esphome::openathan_component::OpenAthan a;begin(updated,a);
    assert(state(updated)=="success" && confirmations==1 && erases==0 && boot_selections==0);
    expect_result(updated,"success");expect_record("","");
    reset();serializeJson(doc,saved_record);running_state=ESP_OTA_IMG_PENDING_VERIFY;a.health=false;
    Upgrade unhealthy;begin(unhealthy,a);now_us=90000000;unhealthy.loop(false);
    assert(confirmations==0 && rollbacks==1);expect_record(descriptor_response,"v0.3.0");
    std::cout<<"Updated application confirms the retained handoff request\n";
  }
  if(std::string(OPENATHAN_FIRMWARE_VERSION)!="v0.2.0") {
    for(const char *expected:{"","v0.3.0"}) {
      reset();JsonDocument doc;doc["schema"]=1;doc["queue"]=descriptor_response;doc["expected"]=expected;
      serializeJson(doc,saved_record);inactive_state=ESP_OTA_IMG_ABORTED;
      Upgrade installed;esphome::openathan_component::OpenAthan a;begin(installed,a);
      const bool fulfilled=std::string(OPENATHAN_FIRMWARE_VERSION)=="v0.3.0";
      assert(state(installed)==(fulfilled?"success":"current"));
      expect_result(installed,fulfilled?"success":"superseded");expect_record("","");
      assert(erases==0 && writes==0 && boot_selections==0 && rollbacks==0);
      assert(action(installed,"cancel")==200 && action(installed,"check")==200);run_worker();assert(state(installed)=="current");
      now_us=0;Upgrade rebooted;begin(rebooted,a);assert(action(rebooted,"cancel")==200 && state(rebooted)=="idle");
    }
    reset();JsonDocument doc;doc["schema"]=1;doc["queue"]=descriptor_response;doc["expected"]="";
    serializeJson(doc,saved_record);signature_valid=false;
    Upgrade corrupt;esphome::openathan_component::OpenAthan a;begin(corrupt,a);
    assert(state(corrupt)=="storage_fault" && action(corrupt,"cancel")==503 && erases==0);
    reset();serializeJson(doc,saved_record);commit_ok=false;
    Upgrade failed_clear;begin(failed_clear,a);assert(state(failed_clear)=="storage_fault");expect_record(descriptor_response,"");
    commit_ok=true;now_us=0;Upgrade retry_clear;begin(retry_clear,a);expect_record("","");assert(action(retry_clear,"cancel")==200);
    std::cout<<"Fulfilled/superseded requests reconcile after preserving USB updates\n";return 0;
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    read_buffer_allocation_fails=true;assert(action(u,"check")==200);run_worker();
    assert(state(u)=="failed" && live_read_buffers==0 && erases==0 && writes==0 && boot_selections==0);
    read_buffer_allocation_fails=false;offer(u);
    assert(live_read_buffers==0 && last_read_buffer_bytes==1024);
    assert(last_read_buffer_caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);
    const auto request=saved_record;read_buffer_allocation_fails=true;u.loop(true);run_worker();
    assert(state(u)=="queued" && saved_record==request && live_read_buffers==0);
    assert(erases==0 && writes==0 && live_ota_handles==0 && boot_selections==0);
    const auto attempts=read_buffer_allocation_attempts;
    for(unsigned i=0;i<20;++i)u.loop(true);
    now_us+=299999000;u.loop(true);assert(read_buffer_allocation_attempts==attempts && !pending_worker);
    read_buffer_allocation_fails=false;now_us+=1000;u.loop(true);run_worker();
    assert(flashed==application_response && live_read_buffers==0 && last_read_buffer_bytes==4096);
    assert(last_read_buffer_caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    assert(saved_record==request && boot_selections==0);
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);assert(action(u,"install","v0.3.0")==409);a.sample.next->utc=1800;u.loop(true);assert(!pending_worker);a.sample.next->utc=9000;u.loop(true);run_worker();assert(flashed==application_response);assert(boot_selections==0);u.loop(true);assert(boot_selections==1 && esphome::App.reboots==1);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);Upgrade restored;begin(restored,a);assert(state(restored)=="queued");assert(action(restored,"cancel")==200);assert(state(restored)=="idle");}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);u.loop(true);callback_update=&u;on_read=cancel_download;run_worker();assert(boot_selections==0 && aborts==1 && erases==1 && writes==0);assert(state(u)=="idle");}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);u.loop(true);callback_update=&u;callback_athan=&a;on_read=prayer_starts;run_worker();assert(boot_selections==0 && aborts==1 && erases==1 && writes==0);assert(state(u)=="queued");}
  for(auto callback:{cancel_download,prayer_starts}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);u.loop(true);
    callback_update=&u;callback_athan=&a;on_open=callback;run_worker();
    assert(erases==0 && writes==0 && aborts==0 && boot_selections==0);
    assert(state(u)==(callback==cancel_download?"idle":"queued"));
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);application_response[20]='b';u.loop(true);run_worker();u.loop(true);assert(boot_selections==0 && aborts==1);assert(state(u)=="queued");}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);image_valid=false;u.loop(true);run_worker();u.loop(true);assert(boot_selections==0);}
  for(const bool allocation_failure:{false,true}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);
    begin_allocation_fails=allocation_failure;begin_erase_fails=!allocation_failure;
    const auto request=saved_record;
    for(unsigned attempt=0;attempt<3;++attempt) {
      u.loop(true);run_worker();assert(state(u)=="queued" && live_ota_handles==0 && saved_record==request);
      assert(aborts==(allocation_failure?0:attempt+1) && writes==0 && boot_selections==0);
      now_us+=300000000;
    }
    assert(action(u,"cancel")==200 && live_ota_handles==0);
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);offer(u);commit_ok=false;assert(action(u,"install","v0.3.0")==503);assert(state(u)=="storage_fault");u.loop(true);assert(!pending_worker && boot_selections==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);signature_valid=false;assert(action(u,"check")==200);run_worker();assert(state(u)=="failed");assert(action(u,"install","v0.3.0")==409);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;a.health=false;running_state=ESP_OTA_IMG_PENDING_VERIFY;begin(u,a);now_us=90000000;u.loop(false);assert(rollbacks==1 && confirmations==0);}
  {reset();saved_record=R"({"schema":1,"queue":"","expected":"v0.3.0"})";inactive_state=ESP_OTA_IMG_ABORTED;
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);assert(state(u)=="rolled_back");assert(boot_selections==0);}
  {reset();saved_record=R"({"schema":1,"queue":"","expected":"v0.3.0"})";
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);assert(state(u)=="failed");
    expect_result(u,"");expect_record("","");assert(boot_selections==0);}
  {reset();saved_record="broken";Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);assert(state(u)=="storage_fault");assert(action(u,"check")==503);}
  {reset();boot_readable=false;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);offer(u);assert(action(u,"install","v0.3.0")==503);assert(boot_selections==0);}
  {reset();running_state=ESP_OTA_IMG_PENDING_VERIFY;Upgrade u;esphome::openathan_component::OpenAthan a;u.begin(&a,false);assert(rollbacks==1 && confirmations==0);}
  for(const auto fault:{openathan::Fault::STORAGE,openathan::Fault::INVALID_SETTINGS,openathan::Fault::INVALID_SCHEDULE,
                        openathan::Fault::AUDIO_UNAVAILABLE,openathan::Fault::PLAYBACK_REJECTED}) {
    reset();running_state=ESP_OTA_IMG_PENDING_VERIFY;Upgrade u;esphome::openathan_component::OpenAthan a;
    a.sample.fault=fault;begin(u,a);assert(confirmations==0);now_us=90000000;u.loop(false);
    assert(confirmations==0 && rollbacks==1);
  }
  {reset();running_state=ESP_OTA_IMG_PENDING_VERIFY;Upgrade u;esphome::openathan_component::OpenAthan a;
    a.clock.valid=false;a.sample.automatic_ready=false;begin(u,a);assert(confirmations==1 && rollbacks==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);
    task_available=false;const auto before=task_attempts;const auto request=saved_record;u.loop(true);
    assert(state(u)=="queued" && task_attempts==before+1 && !pending_worker && saved_record==request);
    for(unsigned i=0;i<20;++i)u.loop(true);
    now_us+=299999000;u.loop(true);assert(task_attempts==before+1);
    now_us+=1000;u.loop(true);assert(task_attempts==before+2 && state(u)=="queued");
    assert(erases==0 && writes==0 && action(u,"cancel")==200 && state(u)=="idle");
    JsonDocument doc;assert(!deserializeJson(doc,saved_record) && doc["queue"].as<std::string>().empty());
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    task_available=false;now_us=60000000;u.loop(true);assert(task_attempts==1 && state(u)=="failed");
    now_us+=299999000;u.loop(true);assert(task_attempts==1);
    now_us+=1000;u.loop(true);assert(task_attempts==2 && !pending_worker);
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);
    // A stale rejection must be invalidated by staging, as in pinned IDF.
    inactive_state=ESP_OTA_IMG_INVALID;u.loop(true);run_worker();
    cut_after_commit=true;try{u.loop(true);assert(false);}catch(const PowerCut&){}
    cut_after_commit=false;assert(boot_selections==0);expect_record(descriptor_response,"v0.3.0");
    now_us=0;Upgrade restarted;begin(restarted,a);assert(state(restarted)=="queued");
    expect_result(restarted,"");expect_record(descriptor_response,"");
    restarted.loop(true);run_worker();restarted.loop(true);assert(boot_selections==1 && esphome::App.reboots==1);
  }
  for(const auto rejection:{ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);u.loop(true);run_worker();
    on_boot_selection=power_cut;try{u.loop(true);assert(false);}catch(const PowerCut&){}
    on_boot_selection=nullptr;assert(boot_selections==1 && esphome::App.reboots==0);expect_record(descriptor_response,"v0.3.0");
    inactive_state=rejection;boot_partition=&running;now_us=0;
    Upgrade restored;begin(restored,a);assert(state(restored)=="rolled_back");expect_result(restored,"rolled_back");
    expect_record("","");restored.loop(true);assert(erases==1 && writes==1 && boot_selections==1);
  }
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue(u);u.loop(true);run_worker();
    on_boot_selection=power_cut;try{u.loop(true);assert(false);}catch(const PowerCut&){}
    on_boot_selection=nullptr;now_us=0;Upgrade handoff;begin(handoff,a);
    assert(state(handoff)=="restarting" && esphome::App.reboots==1);expect_record(descriptor_response,"v0.3.0");
    assert(erases==1 && boot_selections==1);
  }
  {reset();JsonDocument doc;doc["schema"]=1;doc["queue"]=descriptor_response;doc["expected"]="v0.4.0";
    serializeJson(doc,saved_record);Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(state(u)=="storage_fault" && boot_selections==0);
  }
  assert(live_read_buffers==0);
  std::cout<<"Production updater interruption, persistence and rollback checks passed\n";
  return 0;
}

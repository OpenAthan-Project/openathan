// Exercise the production updater with deterministic transport/flash/task/NVS
// adapters. Signature success is injected here; cryptography is tested separately.
#include "upgrade.h"
#include "esphome/core/application.h"
#include <esp_http_client.h>
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
bool commit_ok=true;
int nvs_open(const char *,int,nvs_handle_t *h){*h=1;return 0;}
int nvs_get_blob(nvs_handle_t,const char *,void *out,size_t *size){
  if(saved_record.empty())return ESP_ERR_NVS_NOT_FOUND;
  if(out)memcpy(out,saved_record.data(),saved_record.size());
  *size=saved_record.size();return 0;
}
int nvs_set_blob(nvs_handle_t,const char *,const void *data,size_t size){staged_record.assign(static_cast<const char *>(data),size);return 0;}
int nvs_commit(nvs_handle_t){if(!commit_ok)return -1;saved_record=staged_record;return 0;}
void nvs_close(nvs_handle_t){}
int nvs_erase_all(nvs_handle_t){assert(false);return -1;}
std::string state(Upgrade &update){JsonDocument doc;update.snapshot(doc.to<JsonObject>());return doc["state"].as<std::string>();}
int action(Upgrade &update,const char *name,const char *version=nullptr){JsonDocument doc;auto root=doc.to<JsonObject>();if(version)root["version"]=version;std::string error;return update.action(name,root,error);}
void reset(){now_us=0;saved_record.clear();staged_record.clear();commit_ok=true;signature_valid=true;boot_readable=true;transfer_fail=false;on_open=on_read=nullptr;pending_worker=nullptr;task_available=true;task_attempts=0;boot_selections=aborts=rollbacks=confirmations=erases=writes=0;image_valid=true;running_state=ESP_OTA_IMG_VALID;esphome::App.reboots=0;
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
int main(){
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
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);offer(u);commit_ok=false;assert(action(u,"install","v0.3.0")==503);assert(state(u)=="storage_fault");u.loop(true);assert(!pending_worker && boot_selections==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);signature_valid=false;assert(action(u,"check")==200);run_worker();assert(state(u)=="failed");assert(action(u,"install","v0.3.0")==409);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;a.health=false;running_state=ESP_OTA_IMG_PENDING_VERIFY;begin(u,a);now_us=90000000;u.loop(false);assert(rollbacks==1 && confirmations==0);}
  {reset();saved_record=R"({"schema":1,"queue":"","expected":"v0.3.0"})";Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);assert(state(u)=="rolled_back");assert(boot_selections==0);}
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
  std::cout<<"Production updater interruption, persistence and rollback checks passed\n";
}

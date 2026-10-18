// Real downloader and bank scanner; deterministic transport, flash and NVS.
#include "dua.h"
#include "upgrade.h"
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
using namespace esphome::openathan_device;
using namespace openathan;
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
namespace esphome::openathan_upgrade_qualification {
#include "dua_qualification_transport.inc"
}
static const std::string RELEASE = std::string(OPENATHAN_QUALIFICATION_ORIGIN) + "/releases/download/v0.8.0/";
#else
static const std::string RELEASE = "https://github.com/OpenAthan-Project/openathan/releases/download/v0.8.0/";
#endif
static std::vector<uint8_t> flash, original;
static esp_partition_t partition{AUDIO_PARTITION_SIZE, 0x410000};
static std::map<std::string,std::vector<uint8_t>> nvs, pending;
static int operation{}, cut_at{-1};
static int64_t now_us{};
static std::function<void()> on_write;
struct PowerCut {};
int64_t esp_timer_get_time() { return now_us; }
int nvs_open(const char *,int,nvs_handle_t *handle) { *handle=1;return 0; }
void nvs_close(nvs_handle_t) {}
int nvs_get_blob(nvs_handle_t,const char *key,void *out,size_t *size) {
  if (!nvs.count(key)) return ESP_ERR_NVS_NOT_FOUND;
  auto &v=nvs[key];if(out){assert(*size>=v.size());memcpy(out,v.data(),v.size());}*size=v.size();return 0;
}
int nvs_set_blob(nvs_handle_t,const char *key,const void *data,size_t size) {
  pending[key]=std::vector<uint8_t>(static_cast<const uint8_t *>(data),static_cast<const uint8_t *>(data)+size);return 0;
}
int nvs_commit(nvs_handle_t) { for(auto &[k,v]:pending)nvs[k]=v;pending.clear();return 0; }
int nvs_erase_all(nvs_handle_t) { assert(false);return -1; }
const esp_partition_t *esp_partition_find_first(int,int,const char *name) { assert(std::string(name)=="athan_audio");return &partition; }
int esp_partition_mmap(const esp_partition_t *,size_t,size_t,int,const void **out,esp_partition_mmap_handle_t *h) { *out=flash.data();*h=1;return 0; }
int esp_partition_read(const esp_partition_t *,size_t at,void *out,size_t size) { assert(at+size<=flash.size());memcpy(out,flash.data()+at,size);return 0; }
int esp_partition_erase_range(const esp_partition_t *,size_t at,size_t size) {
  assert(at>=DUA_BANK_START && at%4096==0 && size==4096 && at+size<=flash.size());
  ++operation;std::fill_n(flash.begin()+at,operation==cut_at?size/2:size,0xff);
  if(operation==cut_at)throw PowerCut{};if(on_write)on_write();return 0;
}
int esp_partition_write(const esp_partition_t *,size_t at,const void *data,size_t size) {
  assert(at>=DUA_BANK_START && at+size<=flash.size());++operation;
  const auto *bytes=static_cast<const uint8_t *>(data);
  const size_t count=operation==cut_at?size/2:size;
  for(size_t i=0;i<count;++i){assert((flash[at+i]&bytes[i])==bytes[i]);flash[at+i]&=bytes[i];}
  if(operation==cut_at)throw PowerCut{};if(on_write)on_write();return 0;
}
struct Audio : Playback {
  bool active{}, installed{};const uint8_t *bytes{};size_t size{};
  bool ready() const override {return true;}
  bool playing() const override {return active;}
  bool start(Track) override {return true;}
  void stop() override {active=false;}
  bool dua_supported() const override {return true;}
  bool dua_ready() const override {return installed;}
  bool set_dua_data(const uint8_t *data,size_t length) override {assert(!active);bytes=data;size=length;installed=true;return true;}
};
std::string descriptor() {
  std::string payload=R"({"schema":1,"kind":"dua","version":"v0.8.0","mediaId":"post-athan-dua-v1","layout":"dual-2m-audio-3_5m-v1","format":1,"bytes":4097,"sha256":")";
  uint8_t hash[32];mbedtls_sha256(reinterpret_cast<const uint8_t *>(application_response.data()),application_response.size(),hash,0);
  for(auto byte:hash){payload+="0123456789abcdef"[byte>>4];payload+="0123456789abcdef"[byte&15];}payload+="\"}";
  JsonDocument root;root["payload"]=payload;root["signature"]=std::string(128,'0');std::string out;serializeJson(root,out);return out;
}
void reset() {
  assert(!live_read_buffers);reset_http();flash=original;nvs.clear();pending.clear();operation=0;cut_at=-1;now_us=0;signature_valid=true;on_write={};
  application_response=std::string(4097,'d');
  http_replies[RELEASE+"dua.json"]={200,"",descriptor()};
  http_replies[RELEASE+"dua.mp3"]={200,"",application_response};
}
void enable(esphome::openathan_component::OpenAthan &a,Audio &audio) {a.audio=&audio;a.dua_prefs.begin();assert(a.dua_prefs.update(true,1)==DuaSaveResult::SAVED);}
std::string state(DuaMedia &m) {JsonDocument root;m.snapshot(root.to<JsonObject>());return root["state"].as<std::string>();}
void installed() {
  Audio audio;esphome::openathan_component::OpenAthan athan;enable(athan,audio);DuaMedia media;media.begin(&athan);
  media.loop(true,true);assert(pending_worker);run_worker();media.loop(true,true);assert(audio.installed && state(media)=="ready");
  assert(audio.size==4097 && std::string(reinterpret_cast<const char *>(audio.bytes),audio.size)==application_response);
  assert(std::equal(original.begin(),original.begin()+DUA_BANK_START,flash.begin()));
}
int main(int argc,char **argv) {
  assert(argc==2);std::ifstream in(argv[1],std::ios::binary);original={std::istreambuf_iterator<char>(in),{}};assert(original.size()==AUDIO_PARTITION_SIZE);
#if defined(OPENATHAN_PROVISIONING_TEST_STORAGE) && !defined(OPENATHAN_UPGRADE_QUALIFICATION)
  reset();Audio a;esphome::openathan_component::OpenAthan speaker;enable(speaker,a);DuaMedia blocked;blocked.begin(&speaker);
  blocked.loop(true,true);assert(!pending_worker && !operation && http_requests.empty());
  std::cout<<"Ordinary isolated firmware refuses production dua downloads\n";return 0;
#endif
  reset();installed();const int writes=operation;
  {
    const auto installed_flash=flash;reset_http();const auto before=operation;
    Audio audio;esphome::openathan_component::OpenAthan a;enable(a,audio);DuaMedia m;m.begin(&a);m.loop(true,true);
    assert(audio.installed && flash==installed_flash && operation==before && http_requests.empty() && !pending_worker);
    // A second committed bank wins only while its signed payload is valid.
    auto *second=flash.data()+DUA_BANK_START+DUA_BANK_SIZE;
    std::copy_n(flash.data()+DUA_BANK_START,DUA_BANK_SIZE,second);second[8]=2;
    mbedtls_sha256(second,4060,second+4060,0);
    Audio latest;esphome::openathan_component::OpenAthan b;enable(b,latest);DuaMedia newest;newest.begin(&b);newest.loop(true,true);
    assert(latest.bytes==second+DUA_HEADER_SIZE);
    second[DUA_HEADER_SIZE]^=1;
    Audio fallback;esphome::openathan_component::OpenAthan c;enable(c,fallback);DuaMedia recovered;recovered.begin(&c);recovered.loop(true,true);
    assert(fallback.bytes==flash.data()+DUA_BANK_START+DUA_HEADER_SIZE && http_requests.empty());
  }
  for(int cut=1;cut<=writes;++cut){
    reset();Audio audio;esphome::openathan_component::OpenAthan athan;enable(athan,audio);DuaMedia media;media.begin(&athan);
    cut_at=cut;media.loop(true,true);assert(pending_worker);try{run_worker();assert(false);}catch(PowerCut &){}
    assert(std::equal(original.begin(),original.begin()+DUA_BANK_START,flash.begin()));
    cut_at=-1;Audio after;esphome::openathan_component::OpenAthan boot;enable(boot,after);DuaMedia recovery;recovery.begin(&boot);
    recovery.loop(true,true);if(pending_worker)run_worker();recovery.loop(true,true);assert(after.installed);
  }
  for(unsigned failure=0;failure<5;++failure){
    reset();Audio audio;esphome::openathan_component::OpenAthan athan;enable(athan,audio);DuaMedia media;media.begin(&athan);
    if(failure==0)signature_valid=false;
    if(failure==1)http_replies.begin()->second.data="broken";
    if(failure==2)on_each_read=[&]{media.shutdown();};
    if(failure==3)on_write=[&]{media.shutdown();};
    if(failure==4)http_replies[RELEASE+"dua.mp3"].data="short";
    media.loop(true,true);assert(pending_worker);run_worker();media.loop(false,false);assert(!audio.installed);
    assert(std::equal(original.begin(),original.begin()+DUA_BANK_START,flash.begin()));
  }
  reset();flash[DUA_BANK_START+100]=0;Audio audio;esphome::openathan_component::OpenAthan athan;enable(athan,audio);DuaMedia media;media.begin(&athan);media.loop(true,true);assert(!pending_worker && state(media)=="unavailable" && operation==0);
  std::cout<<"Production dua downloader: "<<writes<<" interrupted write/erase points, recovery, trust, cancellation and bounds passed\n";
}

#define main existing_runtime_tests
#include "upgrade_runtime_tests.cpp"
#undef main

using namespace ::openathan::usb_upgrade;
namespace {
unsigned failures{};
std::string scenario;
void expect(bool value,const std::string &detail) {
  if(!value){++failures;std::cerr<<"FAIL "<<scenario<<": "<<detail<<"\n";}
}
JsonDocument snapshot(Upgrade &u){JsonDocument doc;u.snapshot(doc.to<JsonObject>());return doc;}
std::string identity(){
  JsonDocument envelope,payload;assert(!deserializeJson(envelope,descriptor_response));
  assert(!deserializeJson(payload,envelope["payload"].as<std::string>()));
  return payload["version"].as<std::string>()+":"+payload["sha256"].as<std::string>();
}
void policy(){JsonDocument doc;doc["schema"]=1;doc["enabled"]=true;doc["revision"]=0;
  doc["skipped"]="";doc["request"]=identity();serializeJson(doc,saved_automatic);}
void request(const char *expected="",bool usb=false){JsonDocument doc;doc["schema"]=usb?2:1;
  if(usb)doc["source"]="usb";doc["queue"]=descriptor_response;doc["expected"]=expected;serializeJson(doc,saved_record);}
void hold(){JsonDocument doc;doc["schema"]=1;doc["identity"]=identity();serializeJson(doc,saved_verify_hold);}
bool held(){if(saved_verify_hold.empty())return false;JsonDocument doc;assert(!deserializeJson(doc,saved_verify_hold));
  return !doc["identity"].as<std::string>().empty();}
void automatic_queue(Upgrade &u){u.loop(true);run_worker();u.loop(true);assert(state(u)=="queued");}
void queue_as(Upgrade &u,bool manual){if(manual)queue(u);else automatic_queue(u);}
int preference(Upgrade &u,bool enabled){JsonDocument doc;doc["schema"]=1;doc["enabled"]=enabled;
  doc["expected_revision"]=0;std::string error;return u.action("automatic",doc.as<JsonObject>(),error);}
std::string begin_usb(Upgrade &u){std::vector<std::string> reply;
  assert(u.usb_command(BEGIN,{std::to_string(descriptor_response.size())},reply)==0);
  const auto token=std::stoull(reply[0],nullptr,16);
  for(size_t offset=0;offset<descriptor_response.size();offset+=CHUNK){
    const auto bytes=std::span(reinterpret_cast<const uint8_t *>(descriptor_response.data()+offset),
        std::min(CHUNK,descriptor_response.size()-offset));
    assert(u.usb_chunk({token,0,uint32_t(offset),bytes}));
  }
  const auto text=reply[0];assert(u.usb_command(VERIFY,{text},reply)==0);return text;
}
void clear_power_loss(){pending_worker=nullptr;live_ota_handles=0;staged_request_dirty=staged_automatic_dirty=staged_verify_hold_dirty=false;}
void cancellation_read_fault(){
  reset();policy();request();const auto original=saved_record;
  nvs_read_failure_key="automatic";Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  const auto result=action(u,"cancel");
  expect(result==503,"cancellation returned "+std::to_string(result)+", expected503");
  expect(saved_record==original,"failed cancellation cleared its durable request");
  expect(!snapshot(u)["error"].as<std::string>().empty(),"failed cancellation has no actionable status error");
}
void usb_policy_fault(bool rollback){
  reset();request(rollback?"v0.3.0":"",true);const auto original=saved_record;
  inactive_state=rollback?ESP_OTA_IMG_ABORTED:ESP_OTA_IMG_UNDEFINED;
  nvs_read_failure_key="automatic";Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  if(!rollback){std::vector<std::string> reply;const auto result=u.usb_command(ABORT,{"interrupted"},reply);
    expect(result==255,"USB abort acknowledged without durable suppression");}
  expect(saved_record==original&&u.usb_busy(),"unreadable policy lost durable USB ownership");
}
void rollback_read_fault(unsigned fault){
  reset();policy();request("v0.3.0");inactive_state=ESP_OTA_IMG_ABORTED;
  const auto original=saved_record;
  if(fault==0)description_query_fails=true;
  if(fault==1)inactive_state_result=-1;
  if(fault==2)boot_query_fails=true;
  if(fault==3)candidate_query_fails=true;
  if(fault==4)raw_read_fails=true;
  Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  expect(saved_record==original,"unresolved handoff cleared its journal");
  expect(action(u,"cancel")==409&&preference(u,false)==409,"unresolved handoff allowed owner changes");
  description_query_fails=boot_query_fails=candidate_query_fails=raw_read_fails=false;inactive_state_result=ESP_OK;
  now_us+=5000000;u.loop(true);
  if(pending_worker)run_worker();
  expect(erases==0&&boot_selections==0,"read recovery repeated a rejected image");
  expect(snapshot(u)["automatic_updates"]["skipped_version"]=="v0.3.0","recovered rejection was not suppressed");
}
void uncertain_selection(bool manual,bool selected){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue_as(u,manual);
  u.loop(true);run_worker();selection_fails=true;selection_writes_before_failure=selected;u.loop(true);
  JsonDocument record;assert(!deserializeJson(record,saved_record));
  expect(record["expected"]=="v0.3.0","selection error discarded the handoff marker");
  expect(action(u,"cancel")==409&&preference(u,false)==409,"selection uncertainty allowed owner changes");
  selection_fails=selection_writes_before_failure=false;now_us+=5000000;u.loop(true);
  if(pending_worker)run_worker();
  if(selected)expect(erases==1&&boot_selections==1,"selected application was erased or selected again");
  else {u.loop(true);if(pending_worker)run_worker();u.loop(true);
    expect(boot_selections==2,"positive preselection interruption did not recover");}
}
void rejected_write_fault(unsigned kind,bool usb){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  std::string token;
  if(usb)token=begin_usb(u);else automatic_queue(u);
  const auto original=saved_record;
  if(kind==0)application_response[20]='b';
  if(kind==1)image_valid=false;
  if(kind==2)inactive_version="v0.2.1";
  if(kind==3)application_response[0]=0;
  nvs_write_failure_key="automatic";
  if(usb){
    bool accepted=true;const auto value=std::stoull(token,nullptr,16);
    for(size_t offset=0;offset<application_response.size()&&accepted;offset+=CHUNK){
      const auto bytes=std::span(reinterpret_cast<const uint8_t *>(application_response.data()+offset),
          std::min(CHUNK,application_response.size()-offset));
      accepted=u.usb_chunk({value,1,uint32_t(offset),bytes});
    }
    if(accepted){std::vector<std::string> reply;u.usb_command(FINISH,{token},reply);}
  }else {u.loop(true);run_worker();}
  expect(held(),"validation failure lost its predecision hold");
  expect(saved_record==original,"failed suppression cleared its original request");
  expect(live_ota_handles==0,"failed rejection retained an OTA handle");
  u.shutdown();clear_power_loss();nvs_write_failure_key.clear();now_us=0;
  Upgrade reboot;begin(reboot,a);
  expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0","restart revived a rejected attempt");
  const auto count=erases;reboot.loop(true);if(pending_worker)run_worker();
  expect(erases==count&&boot_selections==0,"restart downloaded or selected the failed attempt");
}
void recovered_hold(){
  reset();policy();request();hold();running_state=ESP_OTA_IMG_PENDING_VERIFY;
  Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  expect(confirmations==1,"verification hold prevented healthy startup confirmation");
  expect(snapshot(u)["automatic_updates"]["skipped_version"]=="v0.3.0","matching recovered hold did not suppress its release");
  JsonDocument journal;assert(!deserializeJson(journal,saved_record));
  expect(journal["queue"]=="","matching recovered hold did not retire the request");
}
void hold_arm_failure(){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);automatic_queue(u);
  nvs_write_failure_key="verify_hold";u.loop(true);run_worker();
  expect(writes==0&&boot_selections==0,"validation ran before its hold could commit");
}
void ordinary_partial_retry(){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);automatic_queue(u);
  const std::string url="https://github.com/OpenAthan-Project/openathan/releases/download/v0.3.0/firmware.ota.bin";
  http_replies[url]={200,"",application_response,{256,-1}};u.loop(true);run_worker();
  expect(!held()&&writes==1,"partial network interruption retained a verification hold");
  http_replies.erase(url);now_us+=300000000;u.loop(true);if(pending_worker)run_worker();u.loop(true);
  expect(boot_selections==1,"ordinary partial download stopped automatic retry");
}
void verification_cut(bool manual,unsigned commit){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue_as(u,manual);
  cut_commit_number=nvs_commits+commit;bool cut=false;
  try{u.loop(true);run_worker();}catch(const PowerCut&){cut=true;}
  expect(cut,"verification boundary did not perform its durable commit");
  cut_commit_number=0;u.shutdown();clear_power_loss();now_us=0;
  Upgrade reboot;begin(reboot,a);
  const bool pending=commit==1||commit==3;
  if(pending){
    expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0","ambiguous verification cut retried automatically");
    const auto count=erases;reboot.loop(true);if(pending_worker)run_worker();
    expect(erases==count&&boot_selections==0,"recovered verification cut replayed firmware");
  }else{
    expect(snapshot(reboot)["queue_source"]==(manual?"manual":"automatic"),"successful verification clear changed request ownership");
    reboot.loop(true);if(pending_worker)run_worker();reboot.loop(true);
    expect(boot_selections==1,"cleared successful verification did not resume its request");
  }
}
void rejection_cleanup_cut(bool manual,unsigned commit){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);queue_as(u,manual);
  application_response[20]='b';cut_commit_number=nvs_commits+commit;bool cut=false;
  try{u.loop(true);run_worker();}catch(const PowerCut&){cut=true;}
  expect(cut,"rejection cleanup boundary did not commit");
  cut_commit_number=0;u.shutdown();clear_power_loss();now_us=0;
  Upgrade reboot;begin(reboot,a);const auto count=erases;reboot.loop(true);if(pending_worker)run_worker();
  expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0","cleanup cut lost rejection suppression");
  expect(erases==count&&boot_selections==0,"cleanup cut replayed a rejected image");
}
void hold_commit_failure(unsigned commit){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);automatic_queue(u);
  fail_commit_number=nvs_commits+commit;u.loop(true);run_worker();
  expect(boot_selections==0,"failed verification commit selected firmware");
  if(commit==1)expect(writes==0,"failed hold-arm commit performed image validation");
  else expect(held(),"failed hold-clear commit discarded durable verification evidence");
  fail_commit_number=0;u.shutdown();clear_power_loss();now_us=0;
  Upgrade reboot;begin(reboot,a);
  if(commit>1)expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0","failed clear commit became an automatic retry");
}
void metadata_retry_boundary(int sector){
  reset();policy();request("v0.3.0");inactive_state=ESP_OTA_IMG_ABORTED;raw_read_failure_sector=sector;
  const auto original=saved_record;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  const auto reads=raw_read_calls;expect(reads>0&&saved_record==original,"failed sector read did not retain handoff");
  u.loop(true);now_us+=4999000;u.loop(true);
  expect(raw_read_calls==reads,"unresolved metadata was read before5000ms");
  now_us+=1000;u.loop(true);expect(raw_read_calls>reads,"unresolved metadata was not retried at5000ms");
  raw_read_failure_sector=-1;now_us+=5000000;u.loop(true);
  expect(erases==0&&snapshot(u)["automatic_updates"]["skipped_version"]=="v0.3.0","recovered sector reads replayed a rejected release");
}
void malformed_metadata(unsigned kind){
  reset();policy();request("v0.3.0");inactive_state=ESP_OTA_IMG_ABORTED;raw_metadata_override=true;
  memset(raw_metadata.data(),0xff,sizeof(raw_metadata));
  raw_metadata[0].ota_seq=1;raw_metadata[0].ota_state=ESP_OTA_IMG_VALID;
  raw_metadata[0].crc=bootloader_common_ota_select_crc(&raw_metadata[0]);
  raw_metadata[1].ota_seq=2;raw_metadata[1].ota_state=ESP_OTA_IMG_ABORTED;
  raw_metadata[1].crc=bootloader_common_ota_select_crc(&raw_metadata[1]);
  if(kind==0)raw_metadata[1].crc^=1;
  if(kind==1)raw_metadata[1].ota_state=7;
  if(kind==2){raw_metadata[1].ota_seq=3;raw_metadata[1].ota_state=ESP_OTA_IMG_VALID;
    raw_metadata[1].crc=bootloader_common_ota_select_crc(&raw_metadata[1]);}
  const auto original=saved_record;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  expect(saved_record==original&&erases==0&&action(u,"cancel")==409,"malformed metadata was treated as safe interruption");
}
void bad_hold(bool read_failure){
  reset();policy();request();saved_verify_hold="corrupt";const auto original=saved_record;
  if(read_failure)nvs_read_failure_key="verify_hold";
  running_state=ESP_OTA_IMG_PENDING_VERIFY;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);u.loop(true);
  expect(confirmations==1,"unreadable verification metadata prevented healthy startup");
  expect(!pending_worker&&erases==0&&saved_record==original&&action(u,"cancel")==503,"unreadable hold allowed updates or erased evidence");
}
void orphan_hold(){
  reset();hold();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);u.loop(true);
  expect(held()&&erases==0&&!pending_worker,"orphan hold allowed automatic installation");
  expect(action(u,"cancel")==200&&!held(),"healthy owner cancellation could not retire an orphan hold");
  expect(snapshot(u)["automatic_updates"]["skipped_version"]=="v0.3.0","orphan cancellation did not suppress its version");
  offer(u);expect(action(u,"install","v0.3.0")==200,"explicit manual installation could not retry a retired hold");
}
void mismatched_hold(bool fulfilled){
  reset();policy();request();JsonDocument marker;marker["schema"]=1;
  marker["identity"]="v0.4.0"+identity().substr(identity().find(':'));serializeJson(marker,saved_verify_hold);
  if(fulfilled){JsonDocument envelope,payload;assert(!deserializeJson(envelope,descriptor_response));
    assert(!deserializeJson(payload,envelope["payload"].as<std::string>()));payload["version"]=OPENATHAN_FIRMWARE_VERSION;
    std::string text;serializeJson(payload,text);envelope["payload"]=text;descriptor_response.clear();serializeJson(envelope,descriptor_response);request();}
  const auto original=saved_record,verification=saved_verify_hold;
  Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);u.loop(true);
  expect(!pending_worker&&erases==0&&saved_record==original&&saved_verify_hold==verification,"mismatched evidence was overwritten");
  expect(action(u,"cancel")==503&&preference(u,false)==503,"mismatched hold allowed destructive owner cleanup");
}
void cancellation_during_read(){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);automatic_queue(u);callback_update=&u;
  on_read=cancel_download;u.loop(true);run_worker();
  expect(!held()&&writes==0&&state(u)=="idle","canceled worker armed stale verification evidence");
}
void usb_verification_cut(bool finish,unsigned commit,bool rejected=false){
  reset();esphome::openathan_component::OpenAthan a;bool cut=false;
  {
    Upgrade u;begin(u,a);const auto token=begin_usb(u);const auto value=std::stoull(token,nullptr,16);
    if(rejected)application_response[20]='b';
    if(!finish)cut_commit_number=nvs_commits+commit;
    try{
      for(size_t offset=0;offset<application_response.size();offset+=CHUNK){
        const auto bytes=std::span(reinterpret_cast<const uint8_t *>(application_response.data()+offset),
            std::min(CHUNK,application_response.size()-offset));
        assert(u.usb_chunk({value,1,uint32_t(offset),bytes}));
      }
      if(finish){cut_commit_number=nvs_commits+commit;std::vector<std::string> reply;u.usb_command(FINISH,{token},reply);}
    }catch(const PowerCut&){cut=true;}
    u.shutdown();
  }
  expect(cut,"USB verification boundary did not commit");
  cut_commit_number=0;clear_power_loss();now_us=0;
  Upgrade reboot;begin(reboot,a);const auto count=erases;
  if(rejected&&commit==3){
    expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0"&&reboot.usb_busy()&&state(reboot)=="usb_interrupted",
        "suppressed USB cleanup remainder lost its explicit ownership");
    std::vector<std::string> reply;
    expect(reboot.usb_command(ABORT,{"interrupted"},reply)==0&&!reboot.usb_busy(),"owner could not finish suppressed USB cleanup");
  }else if(rejected||commit==1){
    expect(snapshot(reboot)["automatic_updates"]["skipped_version"]=="v0.3.0"&&!reboot.usb_busy(),
        "recovered USB verification hold was not suppressed and retired");
  }else expect(reboot.usb_busy()&&state(reboot)=="usb_interrupted","cleared USB hold lost explicit transport ownership");
  reboot.loop(true);if(pending_worker)run_worker();
  expect(erases==count&&boot_selections==0,"USB verification cut became a network installation");
}
void legacy_handoff(bool usb,bool fulfilled){
  reset();JsonDocument doc;doc["schema"]=usb?2:1;if(usb)doc["source"]="usb";
  doc["queue"]="";doc["expected"]=fulfilled?OPENATHAN_FIRMWARE_VERSION:"v0.3.0";serializeJson(doc,saved_record);
  if(fulfilled)running_state=ESP_OTA_IMG_PENDING_VERIFY;
  else{inactive_state=ESP_OTA_IMG_NEW;boot_partition=&inactive;}
  Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  if(fulfilled)expect(confirmations==1&&snapshot(u)["result"]=="success"&&!u.usb_busy(),"legacy fulfilled handoff lost startup confirmation");
  else if(usb){u.loop(true);expect(state(u)=="awaiting_power"&&u.usb_busy()&&esphome::App.reboots==0,
      "legacy selected USB handoff became a Wi-Fi restart");}
  else expect(state(u)=="restarting"&&esphome::App.reboots==1,"legacy selected Wi-Fi handoff did not restart safely");
  expect(erases==0&&boot_selections==0,"legacy handoff repeated staging or selection");
}
void legacy_handoff_actions(bool selected){
  reset();JsonDocument journal;journal["schema"]=1;journal["queue"]="";journal["expected"]="v0.3.0";
  serializeJson(journal,saved_record);
  esphome::openathan_component::OpenAthan a;a.clock.valid=false;
  if(selected){boot_partition=&inactive;inactive_state=ESP_OTA_IMG_NEW;}else boot_query_fails=true;
  Upgrade u;begin(u,a);const auto original=saved_record,policy_record=saved_automatic;
  const auto *boot=boot_partition;
  const auto checked=action(u,"check");
  expect(checked==409,"legacy handoff Check returned "+std::to_string(checked)+", expected409");
  if(pending_worker)run_worker();
  if(selected){
    const auto canceled=action(u,"cancel");
    expect(canceled==409,"selected legacy handoff Cancel returned "+std::to_string(canceled)+", expected409");
    expect(preference(u,false)==409,"selected legacy handoff allowed preference changes");
  }else expect(action(u,"install","v0.3.0")==409,"unresolved legacy handoff allowed replacement installation");
  expect(saved_record==original&&saved_automatic==policy_record,"legacy owner actions mutated journal or skip policy");
  expect(boot_partition==boot&&erases==0&&boot_selections==0&&esphome::App.reboots==0,
      "legacy owner actions changed boot metadata");
}
template<class Function>void run(const std::string &name,Function function){
  scenario=name;const auto before=failures;function();
  if(failures==before)std::cout<<"PASS "<<name<<"\n";
}
}
int main(){
  esp_ota_select_entry_t crc_entry{};crc_entry.ota_seq=1;
  assert(bootloader_common_ota_select_crc(&crc_entry)==0x4743989a);
  run("Wi-Fi cancellation policy read fault",cancellation_read_fault);
  run("USB abort policy read fault",[]{usb_policy_fault(false);});
  run("USB rollback policy read fault",[]{usb_policy_fault(true);});
  for(unsigned fault=0;fault<5;++fault)run("rollback read fault "+std::to_string(fault),[=]{rollback_read_fault(fault);});
  for(bool manual:{false,true})for(bool selected:{false,true})
    run(std::string(manual?"manual":"automatic")+" selection error "+(selected?"after":"before")+" write",[=]{uncertain_selection(manual,selected);});
  for(bool usb:{false,true})for(unsigned kind=0;kind<4;++kind)
    run(std::string(usb?"USB":"Wi-Fi")+" failed rejection write "+std::to_string(kind),[=]{rejected_write_fault(kind,usb);});
  run("matching verification hold recovery",recovered_hold);
  run("verification hold arm failure",hold_arm_failure);
  run("ordinary partial download retry",ordinary_partial_retry);
  for(bool manual:{false,true})for(unsigned commit:{1u,2u,3u,4u})
    run(std::string(manual?"manual":"automatic")+" verification commit cut "+std::to_string(commit),[=]{verification_cut(manual,commit);});
  for(bool manual:{false,true})for(unsigned commit:{4u,5u})
    run(std::string(manual?"manual":"automatic")+" rejection cleanup cut "+std::to_string(commit),[=]{rejection_cleanup_cut(manual,commit);});
  for(unsigned commit:{1u,4u})run("verification commit failure "+std::to_string(commit),[=]{hold_commit_failure(commit);});
  for(int sector:{0,1})run("metadata read retry boundary sector "+std::to_string(sector),[=]{metadata_retry_boundary(sector);});
  for(unsigned kind=0;kind<3;++kind)run("malformed raw metadata "+std::to_string(kind),[=]{malformed_metadata(kind);});
  for(bool read_failure:{false,true})run(read_failure?"verification hold read failure":"corrupt verification hold",[=]{bad_hold(read_failure);});
  run("orphan hold owner recovery",orphan_hold);
  for(bool fulfilled:{false,true})run(fulfilled?"fulfilled queue hold mismatch":"queue hold mismatch",[=]{mismatched_hold(fulfilled);});
  run("cancellation precedes worker hold",cancellation_during_read);
  for(bool finish:{false,true})for(unsigned commit:{1u,2u})
    run(std::string(finish?"USB final":"USB first-write")+" verification cut "+std::to_string(commit),[=]{usb_verification_cut(finish,commit);});
  for(unsigned commit:{2u,3u})run("USB rejection cleanup cut "+std::to_string(commit),[=]{usb_verification_cut(true,commit,true);});
  for(bool usb:{false,true})for(bool fulfilled:{false,true})
    run(std::string(usb?"USB":"Wi-Fi")+" legacy "+(fulfilled?"fulfilled":"selected")+" handoff",[=]{legacy_handoff(usb,fulfilled);});
  for(bool selected:{false,true})run(selected?"legacy selected owner actions":"legacy unresolved owner actions",[=]{legacy_handoff_actions(selected);});
  std::cout<<"Recovery regression failures: "<<failures<<"\n";
  return failures?1:0;
}

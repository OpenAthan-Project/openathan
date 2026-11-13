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
  expect(!saved_verify_hold.empty(),"validation failure lost its predecision hold");
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
  expect(saved_verify_hold.empty()&&writes==1,"partial network interruption retained a verification hold");
  http_replies.erase(url);now_us+=300000000;u.loop(true);if(pending_worker)run_worker();u.loop(true);
  expect(boot_selections==1,"ordinary partial download stopped automatic retry");
}
template<class Function>void run(const std::string &name,Function function){
  scenario=name;const auto before=failures;function();
  if(failures==before)std::cout<<"PASS "<<name<<"\n";
}
}
int main(){
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
  std::cout<<"Recovery regression failures: "<<failures<<"\n";
  return failures?1:0;
}

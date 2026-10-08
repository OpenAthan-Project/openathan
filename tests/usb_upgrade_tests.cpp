// Reuse the production updater's deterministic flash/NVS/clock adapters.
#define main existing_ota_tests
#include "upgrade_runtime_tests.cpp"
#undef main
using namespace ::openathan::usb_upgrade;
uint64_t token_value(const std::string &text) { return std::stoull(text, nullptr, 16); }
std::string start_usb(Upgrade &u) {
  std::vector<std::string> reply;
  assert(u.usb_command(BEGIN,{std::to_string(descriptor_response.size())},reply)==0);
  assert(reply.size()==1);
  const auto token=token_value(reply[0]);
  for(size_t offset=0;offset<descriptor_response.size();offset+=CHUNK) {
    auto data=std::span(reinterpret_cast<const uint8_t *>(descriptor_response.data()+offset),std::min(CHUNK,descriptor_response.size()-offset));
    assert(u.usb_chunk({token,0,uint32_t(offset),data}));
  }
  return reply[0];
}
void send_app(Upgrade &u,const std::string &text) {
  for(size_t offset=0;offset<application_response.size();offset+=CHUNK) {
    auto data=std::span(reinterpret_cast<const uint8_t *>(application_response.data()+offset),std::min(CHUNK,application_response.size()-offset));
    assert(u.usb_chunk({token_value(text),1,uint32_t(offset),data}));
  }
}
enum class MetadataFault { BOOT, DESCRIPTION, STATE, MISSING_STATE, REJECTED_STATE, VERSION, CANDIDATE, OTHER_BOOT };
void metadata_fault(MetadataFault fault) {
  switch(fault) {
    case MetadataFault::BOOT: boot_query_fails=true;break;
    case MetadataFault::DESCRIPTION: description_query_fails=true;break;
    case MetadataFault::STATE: inactive_state_result=-1;break;
    case MetadataFault::MISSING_STATE: inactive_state_result=ESP_ERR_NOT_FOUND;break;
    case MetadataFault::REJECTED_STATE: inactive_state=ESP_OTA_IMG_ABORTED;break;
    case MetadataFault::VERSION: inactive_version="v0.2.1";break;
    case MetadataFault::CANDIDATE: candidate_query_fails=true;break;
    case MetadataFault::OTHER_BOOT: {
      static const esp_partition_t other{0x200000,0x410000};boot_partition=&other;break;
    }
  }
}
void restore_selected_metadata() {
  boot_query_fails=description_query_fails=candidate_query_fails=false;
  inactive_state_result=ESP_OK;inactive_state=ESP_OTA_IMG_NEW;inactive_version="v0.3.0";boot_partition=&inactive;
}
void save_usb_request(const char *expected) {
  JsonDocument record;record["schema"]=2;record["source"]="usb";
  record["queue"]=descriptor_response;record["expected"]=expected;serializeJson(record,saved_record);
}
void expect_usb_blocked(Upgrade &u,const std::string &record) {
  const auto commits=nvs_commits,erase_count=erases,write_count=writes,selection_count=boot_selections;
  std::vector<std::string> reply;
  assert(u.usb_busy() && saved_record==record);
  assert(u.usb_command(ABORT,{"interrupted"},reply)==255);
  assert(u.usb_command(BEGIN,{"200"},reply)==255);
  assert(action(u,"check")==409 && action(u,"cancel")==409 && action(u,"install","v0.3.0")==409);
  assert(saved_record==record && nvs_commits==commits && erases==erase_count && writes==write_count && boot_selections==selection_count);
  assert(esphome::App.reboots==0 && http_requests.empty());
}
void check_selection_reads() {
  for(bool failed_finish:{false,true}) for(auto fault:{MetadataFault::BOOT,MetadataFault::DESCRIPTION,
      MetadataFault::STATE,MetadataFault::MISSING_STATE,MetadataFault::REJECTED_STATE,
      MetadataFault::VERSION,MetadataFault::CANDIDATE,MetadataFault::OTHER_BOOT}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);send_app(u,token);
    selection_fails=selection_writes_before_failure=failed_finish;
    assert(u.usb_command(FINISH,{token},reply)==(failed_finish?255:0));
    const auto record=saved_record;const auto commits=nvs_commits;
    const auto erase_count=erases,write_count=writes,selection_count=boot_selections;metadata_fault(fault);
    for(unsigned i=0;i<3;++i) {now_us+=100000000;u.loop(true);assert(state(u)=="usb_selection_uncertain");expect_usb_blocked(u,record);}
    assert(nvs_commits==commits && erases==erase_count && writes==write_count && boot_selections==selection_count);
    Upgrade restarted;begin(restarted,a);
    assert(state(restarted)=="usb_selection_uncertain");expect_usb_blocked(restarted,record);
    restore_selected_metadata();restarted.loop(true);
    assert(state(restarted)=="awaiting_power" && nvs_commits==commits);expect_usb_blocked(restarted,record);
    assert(erases==erase_count && writes==write_count && boot_selections==selection_count);
  }
  // A previous firmware may already have dropped its handoff marker. Empty
  // expected is never sufficient evidence that its selected image is discardable.
  for(bool unreadable:{false,true}) {
    reset();save_usb_request("");boot_partition=&inactive;inactive_state=ESP_OTA_IMG_NEW;
    boot_query_fails=unreadable;const auto record=saved_record;
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(state(u)==(unreadable?"usb_selection_uncertain":"awaiting_power"));expect_usb_blocked(u,record);
    restore_selected_metadata();u.loop(true);assert(state(u)=="awaiting_power");expect_usb_blocked(u,record);
  }
  // Missing inactive metadata is normal after esp_ota_begin. A successfully
  // read current boot selection permits recovery, including a partial image.
  for(const char *expected:{"","v0.3.0"}) {
    reset();save_usb_request(expected);inactive_state_result=ESP_ERR_NOT_FOUND;
    description_query_fails=std::string(expected).empty();
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(state(u)=="usb_interrupted");expect_record(descriptor_response,"");
    std::vector<std::string> reply;assert(u.usb_command(ABORT,{"interrupted"},reply)==0);
    assert(!u.usb_busy() && erases==0 && writes==0 && boot_selections==0 && http_requests.empty());
  }
  // Status is only an observation: discard must re-read metadata at command time.
  for(bool selected:{false,true}) {
    reset();save_usb_request("");Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    assert(state(u)=="usb_interrupted");const auto record=saved_record;
    if(selected) {boot_partition=&inactive;inactive_state=ESP_OTA_IMG_NEW;} else boot_query_fails=true;
    expect_usb_blocked(u,record);
    assert(state(u)==(selected?"awaiting_power":"usb_selection_uncertain"));
    boot_query_fails=false;boot_partition=&running;u.loop(true);assert(state(u)=="usb_interrupted");
    std::vector<std::string> reply;assert(u.usb_command(ABORT,{"interrupted"},reply)==0 && !u.usb_busy());
  }
  for(auto rejection:{ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED}) {
    reset();save_usb_request("v0.3.0");inactive_state=rejection;
    Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    expect_result(u,"rolled_back");assert(!u.usb_busy() && boot_selections==0 && writes==0);
  }
  reset();save_usb_request("");Upgrade pending;esphome::openathan_component::OpenAthan a;
  pending.begin(&a,true);const auto record=saved_record;std::vector<std::string> reply;
  assert(pending.usb_command(ABORT,{"interrupted"},reply)==255 && saved_record==record);
}
int main() {
  if(std::string(OPENATHAN_FIRMWARE_VERSION)=="v0.4.0") {
    // A maintainer-installed newer app can supersede a USB request, but an
    // older candidate may still be selected. Supersession must not erase proof.
    for(const char *expected:{"","v0.3.0"}) for(bool unreadable:{false,true}) {
      reset();save_usb_request(expected);boot_partition=&inactive;inactive_state=ESP_OTA_IMG_NEW;
      boot_query_fails=unreadable;const auto record=saved_record;
      Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
      assert(state(u)==(unreadable?"usb_selection_uncertain":"awaiting_power"));expect_usb_blocked(u,record);
      restore_selected_metadata();u.loop(true);assert(state(u)=="awaiting_power");expect_usb_blocked(u,record);
      boot_partition=&running;u.loop(true);assert(state(u)=="current" && !u.usb_busy());
      expect_result(u,"superseded");expect_record("","");
      assert(erases==0 && writes==0 && boot_selections==0 && esphome::App.reboots==0 && http_requests.empty());
    }
    std::cout<<"Superseded USB requests require positive boot-selection reconciliation\n";return 0;
  }
  if(std::string(OPENATHAN_FIRMWARE_VERSION)=="v0.3.0") {
    for(const char *expected:{"","v0.3.0"}) {
      reset();save_usb_request(expected);running_state=ESP_OTA_IMG_PENDING_VERIFY;
      Upgrade u;esphome::openathan_component::OpenAthan a;u.begin(&a,true);
      const auto record=saved_record;std::vector<std::string> reply;
      assert(u.usb_command(ABORT,{"interrupted"},reply)==255 && saved_record==record);
      now_us=30000000;u.loop(false);
      expect_result(u,"success");assert(confirmations==1&&boot_selections==0&&writes==0);
      assert(!u.usb_busy());expect_record("","");
      reset();save_usb_request(expected);running_state=ESP_OTA_IMG_PENDING_VERIFY;
      Upgrade unhealthy;a.health=false;begin(unhealthy,a);const auto pending_record=saved_record;
      assert(unhealthy.usb_command(ABORT,{"interrupted"},reply)==255 && saved_record==pending_record);
      now_us=90000000;unhealthy.loop(false);assert(confirmations==0 && rollbacks==1 && saved_record==pending_record);
    }
    std::cout<<"USB handoff confirms only after startup health\n";return 0;
  }
  check_selection_reads();
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);signature_valid=false;
    const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==1);assert(erases==0&&writes==0&&saved_record.empty());}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);a.clock.valid=false;a.health=false;
    std::vector<std::string> reply;assert(u.usb_command(BEGIN,{"200"},reply)==255&&erases==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);assert(erases==1&&writes==0);assert(action(u,"check")==409);
    auto data=std::span(reinterpret_cast<const uint8_t *>(application_response.data()),size_t{CHUNK});
    assert(!u.usb_chunk({token_value(token),1,1,data}));assert(!u.usb_chunk({token_value(token)+1,1,0,data}));
    assert(u.usb_chunk({token_value(token),1,0,data}));assert(!u.usb_chunk({token_value(token),1,0,data}));
    assert(u.usb_command(ABORT,{token},reply)==0&&live_ota_handles==0&&boot_selections==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);a.clock.valid=false;
    const auto token=start_usb(u);std::vector<std::string> reply;assert(u.usb_command(VERIFY,{token},reply)==0);
    send_app(u,token);assert(u.usb_command(FINISH,{token},reply)==0);assert(reply[0]=="verified");assert(state(u)=="awaiting_power"&&boot_selections==1);
    const auto record=saved_record;now_us+=100000000;u.loop(true);assert(saved_record==record&&esphome::App.reboots==0&&http_requests.empty());}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);application_response[0]='b';send_app(u,token);
    assert(u.usb_command(FINISH,{token},reply)==1&&boot_selections==0&&live_ota_handles==0);}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);now_us+=41000000;u.loop(false);assert(aborts==1&&boot_selections==0&&!u.usb_busy());}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);send_app(u,token);cut_after_commit=true;
    try{u.usb_command(FINISH,{token},reply);assert(false);}catch(const PowerCut&){}cut_after_commit=false;
    Upgrade restarted;begin(restarted,a);restarted.loop(true);assert(state(restarted)=="usb_interrupted"&&http_requests.empty()&&boot_selections==0);
    assert(restarted.usb_command(ABORT,{"interrupted"},reply)==0&&!restarted.usb_busy());}
  {reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);send_app(u,token);assert(u.usb_command(FINISH,{token},reply)==0);
    inactive_state=ESP_OTA_IMG_ABORTED;boot_partition=&running;Upgrade restarted;begin(restarted,a);
    expect_result(restarted,"rolled_back");assert(!restarted.usb_busy()&&http_requests.empty());}
  for(bool selected : {false,true}) {
    reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);const auto token=start_usb(u);std::vector<std::string> reply;
    assert(u.usb_command(VERIFY,{token},reply)==0);send_app(u,token);
    selection_fails=true;selection_writes_before_failure=selected;
    assert(u.usb_command(FINISH,{token},reply)==255&&state(u)=="usb_selection_uncertain");
    assert(!saved_record.empty()&&u.usb_busy());u.loop(true);
    assert(state(u)==(selected?"awaiting_power":"usb_interrupted")&&http_requests.empty()&&esphome::App.reboots==0);
    selection_fails=false;selection_writes_before_failure=false;
  }
  std::cout<<"USB signature, transfer ordering, power handoff, timeout and rollback checks passed\n";
}

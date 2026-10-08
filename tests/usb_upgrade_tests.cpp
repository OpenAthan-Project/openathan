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
int main() {
  if(std::string(OPENATHAN_FIRMWARE_VERSION)=="v0.3.0") {
    reset();JsonDocument record;record["schema"]=2;record["source"]="usb";record["queue"]=descriptor_response;record["expected"]="v0.3.0";serializeJson(record,saved_record);
    running_state=ESP_OTA_IMG_PENDING_VERIFY;Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
    expect_result(u,"success");assert(confirmations==1&&boot_selections==0&&writes==0);
    assert(!u.usb_busy());std::cout<<"USB handoff confirms only after startup health\n";return 0;
  }
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

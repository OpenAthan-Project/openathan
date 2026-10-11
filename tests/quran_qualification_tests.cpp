#include "quran.h"
#include "../firmware/esphome/components/openathan_audio/quran_http.h"
#include "../firmware/esphome/components/openathan_quran_qualification/qualification.h"
#include <cassert>
#include <iostream>

int64_t esp_timer_get_time() { return 1000000; }
using esphome::openathan_device::QuranHttp;
namespace fixture = esphome::openathan_quran_qualification;
int main() {
  const std::string logical="https://cdn.mp3quran.net/qualification/001.mp3";
  const std::string mapped=std::string(OPENATHAN_QURAN_QUALIFICATION_ORIGIN)+
    "/OPENATHAN_QURAN_QUALIFICATION_V1/cdn.mp3quran.net/qualification/001.mp3";
  http_replies[mapped]={200,"","invalid audio",{},false,"audio/mpeg"};
  {QuranHttp response;assert(response.open(logical,true));assert(response.content_type=="audio/mpeg");}
  assert(http_requests.size()==1&&http_requests[0].url==mapped&&!last_bundle&&
    last_ca==OPENATHAN_QURAN_QUALIFICATION_CA&&http_requests[0].auto_redirect_disabled&&live_http_clients==0);
  // The mapping never authorizes a private URL or a forbidden logical redirect.
  for(const auto &destination:{"http://cdn.mp3quran.net/qualification/001.mp3","https://evil.example/a.mp3",
                              "https://192.168.1.2:8443/a.mp3"}){
    reset_http();http_replies[mapped]={302,destination,""};
    {QuranHttp response;assert(!response.open(logical,true));}
    assert(http_requests.size()==1&&live_http_clients==0);
  }
  reset_http();{QuranHttp response;assert(!response.open(OPENATHAN_QURAN_QUALIFICATION_ORIGIN "/audio",true));}
  assert(http_requests.empty());
  reset_http();http_replies[mapped]={302,logical,""};
  {QuranHttp response;assert(!response.open(logical,true));}
  assert(http_requests.size()==5&&live_http_clients==0);
  const std::string metadata="https://www.mp3quran.net/api/v3/reciters?language=eng";
  const auto target=fixture::route(metadata);reset_http();http_replies[target]={200,"","{\"reciters\":[]}"};
  {QuranHttp response;assert(response.open(metadata));}
  fixture::worker_finished(4096);fixture::worker_finished(3056);fixture::worker_finished(3900);
  JsonDocument state;fixture::snapshot(state.to<JsonObject>());
  assert(state["qualification"]["identity"]=="OPENATHAN_QURAN_QUALIFICATION_V1"&&
         state["qualification"]["metadata_stack_min_free_bytes"]==3056&&
         state["qualification"]["metadata_jobs"]==3);
  std::cout<<"Quran qualification routing, original redirect policy, isolated trust and observations passed\n";
}

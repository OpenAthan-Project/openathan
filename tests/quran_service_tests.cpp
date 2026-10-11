#include "quran.h"
#include "../firmware/esphome/components/openathan_audio/quran_policy.h"
#include "../firmware/esphome/components/openathan_audio/quran_http.h"
#include <esp_http_client.h>
#include <freertos/task.h>
#include <cassert>
#include <iostream>

int64_t clock_us = 1000000;
int64_t esp_timer_get_time() { return clock_us; }
using namespace esphome::openathan_device;
struct Audio : openathan::Playback {
  bool active{}, available{true}; uint32_t epoch{}; std::string url;
  openathan::PlaybackSource kind{openathan::PlaybackSource::NONE};
  bool ready() const override { return available; }
  bool playing() const override { return active; }
  bool stream_supported() const override { return true; }
  uint32_t playback_epoch() const override { return epoch; }
  openathan::PlaybackSource source() const override { return kind; }
  bool start(openathan::Track) override { ++epoch; active = true; kind = openathan::PlaybackSource::ATHAN; return true; }
  void stop() override { ++epoch; active = false; kind = openathan::PlaybackSource::NONE; }
  bool start_stream(const std::string &value) override { url = value; active = true; kind = openathan::PlaybackSource::QURAN; return true; }
};
JsonDocument input(const char *text) { JsonDocument doc; assert(!deserializeJson(doc, text)); return doc; }
int request_play(Quran &service, JsonDocument &selection, std::string &error) {
  JsonDocument state; service.snapshot(state.to<JsonObject>());
  selection["expected_playback_token"] = state["playback_token"];
  return service.action("play", selection.as<JsonObjectConst>(), error);
}
void execute(Quran &service) { run_worker(); service.loop(true, false); assert(live_http_clients == 0); }
void failure_cases() {
  const std::string metadata_url = "https://www.mp3quran.net/api/v3/reciters?language=eng&reciter=1";
  const std::string valid = R"({"reciters":[{"id":1,"moshaf":[{"id":9,"name":"Fixture","surah_list":"1","server":"https://cdn.mp3quran.net/example/"}]}]})";
  for (const auto failure : {HttpFailure::INIT, HttpFailure::OPEN, HttpFailure::HEADERS}) {
    Audio audio; esphome::openathan_component::OpenAthan athan; athan.audio=&audio;
    Quran service; service.begin(&athan,"failure");service.loop(true,false);reset_http();
    http_replies[metadata_url]={200,"",valid};http_failure=failure;http_failure_url=metadata_url;
    auto play=input(R"({"reciter":1,"edition":9,"surah":1})");std::string error;
    assert(request_play(service,play,error)==200);execute(service);
    JsonDocument state;service.snapshot(state.to<JsonObject>());
    assert(state["state"]=="error"&&!audio.active&&last_bundle);
    // Time/loop changes cannot retry a failed request. A fresh explicit Play can.
    const auto count=http_requests.size();for(int i=0;i<10;++i){clock_us+=10000000;service.loop(true,false);}
    assert(http_requests.size()==count&&!audio.active);
    http_failure=HttpFailure::NONE;assert(request_play(service,play,error)==200);execute(service);assert(audio.active);
  }
  for (unsigned scenario=0;scenario<7;++scenario) {
    reset_http();clock_us=1000000;
    Audio audio;esphome::openathan_component::OpenAthan athan;athan.audio=&audio;
    Quran service;service.begin(&athan,"failure");service.loop(true,false);
    auto &reply=http_replies[metadata_url];reply.data=valid;
    if(scenario==0)reply.data=R"({"reciters":[{"id":)";
    if(scenario==1){reply.data=valid.substr(0,20);reply.incomplete=true;}
    if(scenario==2)reply.read_steps={8,-1};
    if(scenario==3){reply.read_steps={8,0};reply.incomplete=true;}
    if(scenario==4){reply.read_steps=std::vector<int>(10,1);on_each_read=[](){clock_us+=10000000;};}
    if(scenario==5){reply.read_steps=std::vector<int>(10,1);
      on_each_read=[&](){athan.stop();service.loop(true,false);on_each_read=nullptr;};}
    if(scenario==6)reply.status=503;
    auto play=input(R"({"reciter":1,"edition":9,"surah":1})");std::string error;
    assert(request_play(service,play,error)==200);execute(service);
    JsonDocument state;service.snapshot(state.to<JsonObject>());
    assert(!audio.active&&audio.url.empty());assert(state["state"]==(scenario==5?"idle":"error"));
    if(scenario==4)assert(clock_us<=41000000); // Absolute deadline stops slow trickles.
    const auto calls=http_requests.size();for(int i=0;i<10;++i){clock_us+=10000000;service.loop(true,false);}
    assert(http_requests.size()==calls);
    on_each_read=nullptr;reply={200,"",valid};clock_us=1000000;
    assert(request_play(service,play,error)==200);execute(service);assert(audio.active);
  }
  reset_http();
}
int main() {
  failure_cases();
  assert(quran_audio_url("https://cdn.mp3quran.net/audio/example/001.mp3"));
  assert(quran_audio_url("https://server15.mp3quran.net/example/001.mp3"));
  for (const char *url : {"http://cdn.mp3quran.net/a.mp3", "https://cdn.mp3quran.net.evil/a.mp3",
      "https://cdn.mp3quran.net:443/a.mp3", "https://cdn.mp3quran.net/../a.mp3",
      "https://cdn.mp3quran.net/%2f.mp3", "https://127.0.0.1/a.mp3", "https://server.mp3quran.net/a.mp3"})
    assert(!quran_audio_url(url));
  QuranObjectStream parser("reciters"); std::string object;
  unsigned objects_seen = 0;
  for (char c : std::string("{\"reciters\":[{\"name\":\"A } \\\"\"},{\"id\":2}]}")) {
    assert(parser.feed(c, object)); if (!object.empty()) ++objects_seen;
  }
  assert(parser.complete() && objects_seen == 2);
  for (const std::string text : {"{\"reciters\":[{},]}", "{\"reciters\":[{}]}x", "{\"reciters\":[", "{\"re citers\":[]}"}) {
    QuranObjectStream bad("reciters"); bool accepted = true;
    for (char c : text) accepted = bad.feed(c, object) && accepted;
    assert(!accepted || !bad.complete());
  }
  QuranObjectStream big("reciters"); bool accepted = true;
  for (char c : std::string("{\"reciters\":[{\"name\":\"") + std::string(16384, 'a')) accepted = big.feed(c, object) && accepted;
  assert(!accepted);
  Audio audio;
  esphome::openathan_component::OpenAthan athan; athan.audio = &audio;
  Quran service; service.begin(&athan, "boot-a"); service.loop(true, false);
  const std::string base = "https://www.mp3quran.net/api/v3/";
  const std::string entry = R"({"id":1,"name":"Example","moshaf":[{"id":9,"name":"Edition","server":"https://cdn.mp3quran.net/example/","surah_list":"1,18,114"}]})";
  http_replies[base + "reciters?language=eng&reciter=1"] = {200, "", "{\"reciters\":[" + entry + "]}"};
  std::string error;
  auto play = input(R"({"reciter":1,"edition":9,"surah":18})");
  // Reject a Play that arrives after Stop without allocating a worker or
  // disturbing current playback. A token is required even for older clients.
  const auto attempts = task_attempts;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  JsonDocument before; service.snapshot(before.to<JsonObject>());
  play["expected_playback_token"] = before["playback_token"];
  audio.stop();
  const auto stopped_epoch = audio.epoch;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  assert(audio.epoch == stopped_epoch && task_attempts == attempts && !pending_worker);
  play["expected_playback_token"] = 1;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 400);
  play["expected_playback_token"] = std::string(60, 'x');
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 400);
  play["expected_playback_token"] = before["playback_token"];
  play["unexpected"] = true;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 400);
  play.remove("unexpected");
  audio.start(openathan::Track::NORMAL);
  const auto athan_epoch = audio.epoch;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  assert(audio.epoch == athan_epoch && audio.kind == openathan::PlaybackSource::ATHAN && task_attempts == attempts);
  audio.stop(); // Even after Athan ends, an older Play cannot revive.
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  service.snapshot(before.to<JsonObject>());
  play["expected_playback_token"] = before["playback_token"];
  service.begin(&athan, "boot-b");
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  audio.epoch = UINT32_MAX;
  service.snapshot(before.to<JsonObject>());
  play["expected_playback_token"] = before["playback_token"];
  audio.stop();
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  assert(audio.epoch == 0 && task_attempts == attempts);
  assert(request_play(service, play, error) == 200);
  athan.stop(); execute(service); assert(audio.url.empty()); // Stop while resolving.
  assert(request_play(service, play, error) == 200);
  service.loop(false, false); execute(service); assert(audio.url.empty()); // Reconnect cannot revive a canceled job.
  assert(request_play(service, play, error) == 200);
  service.shutdown(); execute(service); assert(audio.url.empty());
  assert(request_play(service, play, error) == 200);
  service.loop(true, true); execute(service); assert(audio.url.empty()); // Maintenance beats late results.
  assert(request_play(service, play, error) == 200);
  audio.start(openathan::Track::NORMAL); execute(service); assert(audio.url.empty()); // Athan beats late result.
  audio.active = false; // Athan has reserved ownership while its Stop is still draining.
  assert(request_play(service, play, error) == 409);
  service.loop(true, false);
  assert(request_play(service, play, error) == 409);
  audio.active = true; // The reservation must remain protected after playback starts.
  assert(request_play(service, play, error) == 409);
  audio.stop();
  assert(request_play(service, play, error) == 200);
  execute(service); assert(audio.url == "https://cdn.mp3quran.net/example/018.mp3");
  service.loop(false, false);assert(!audio.active && audio.kind==openathan::PlaybackSource::NONE);
  service.loop(true, false);assert(!audio.active);
  assert(request_play(service, play, error) == 200);execute(service);
  audio.available=false;service.loop(true, false);assert(!audio.active);audio.available=true;
  audio.stop(); audio.url.clear();
  auto unavailable = input(R"({"reciter":1,"edition":9,"surah":2})");
  assert(request_play(service, unavailable, error) == 200); execute(service);
  JsonDocument state; service.snapshot(state.to<JsonObject>()); assert(state["state"] == "error" && audio.url.empty());
  service.loop(true, true); assert(request_play(service, play, error) == 409);
  service.loop(false, false); assert(request_play(service, play, error) == 503);
  service.loop(true, false);
  task_available = false;
  assert(request_play(service, play, error) == 503); task_available = true;
  // The catalog is scanned entry by entry; pagination and search cover all rows.
  audio.stop(); reset_http();
  std::string reciters = "{\"reciters\":[";
  for (unsigned i = 1; i <= 25; ++i) {
    if (i > 1) reciters += ',';
    reciters += "{\"id\":" + std::to_string(i) + ",\"name\":\"Reciter " + std::to_string(i) + "\"}";
  }
  reciters += "]}";
  http_replies[base + "reciters?language=eng"] = {200, "", reciters};
  std::string chapters = "{\"suwar\":[";
  for (unsigned i = 1; i <= 114; ++i) {
    if (i > 1) chapters += ',';
    chapters += "{\"id\":" + std::to_string(i) + ",\"name\":\"Surah " + std::to_string(i) + "\"}";
  }
  chapters += "]}";
  http_replies[base + "suwar?language=eng"] = {200, "", chapters};
  auto catalog = input(R"({"kind":"reciters","offset":0,"query":""})");
  assert(service.action("catalog", catalog.as<JsonObjectConst>(), error) == 200); execute(service);
  state.clear(); service.catalog_snapshot(state.to<JsonObject>());
  assert(state["state"] == "ready" && state["data"]["total"] == 25 && state["data"]["reciters"].size() == 20);
  assert(state["data"]["surahs"].size() == 114 && state["data"]["next_offset"] == 20);
  catalog["offset"] = 20;
  assert(service.action("catalog", catalog.as<JsonObjectConst>(), error) == 200); execute(service);
  state.clear(); service.catalog_snapshot(state.to<JsonObject>());
  assert(state["data"]["reciters"].size() == 5 && state["data"]["next_offset"].isNull());
  catalog["offset"] = 0; catalog["query"] = "RECITER 25";
  audio.start_stream("https://cdn.mp3quran.net/example/018.mp3");const auto epoch=audio.epoch;
  assert(service.action("catalog", catalog.as<JsonObjectConst>(), error) == 200); execute(service);
  assert(audio.active&&audio.epoch==epoch); // Catalog browsing cannot stop current audio.
  state.clear(); service.catalog_snapshot(state.to<JsonObject>());
  assert(state["data"]["total"] == 1 && state["data"]["reciters"][0]["id"] == 25);
  // Redirect destination is rejected before opening its connection.
  reset_http();
  http_replies[base + "reciters?language=eng&reciter=1"] = {302, "https://evil.example/reciters", ""};
  assert(request_play(service, play, error) == 200); execute(service);
  assert(http_requests.size() == 1 && last_bundle && http_requests[0].auto_redirect_disabled);
  reset_http();
  const std::string audio_url = "https://cdn.mp3quran.net/example/018.mp3";
  const std::string redirected = "https://server15.mp3quran.net/example/018.mp3";
  http_replies[audio_url] = {302, redirected, ""};
  http_replies[redirected] = {200, "", "mp3"};
  { QuranHttp response; assert(response.open(audio_url, true)); }
  assert(http_requests.size() == 2 && live_http_clients == 0);
  { QuranHttp response; assert(response.open(redirected,true));auto *client=response.release();
    assert(client->config.user_data==nullptr);
    char key[]="Content-Type",value[]="audio/mpeg";
    esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,key,value,client->config.user_data};
    assert(client->config.event_handler(&event)==0);esp_http_client_cleanup(client);
  }
  reset_http();
  for (const auto &destination : {"http://cdn.mp3quran.net/example/018.mp3", "https://evil.example/018.mp3"}) {
    http_replies[audio_url] = {302, destination, ""};
    { QuranHttp response; assert(!response.open(audio_url, true)); }
    assert(http_requests.back().url == audio_url && live_http_clients == 0);
  }
  std::cout << "Quran parser, URL bounds, cancellation, priority, availability and HTTPS policy passed\n";
}

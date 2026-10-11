#include "quran.h"
#include "../firmware/esphome/components/openathan_audio/quran_policy.h"
#include "../firmware/esphome/components/openathan_audio/quran_http.h"
#include <esp_http_client.h>
#include <freertos/task.h>
#include <cassert>
#include <iostream>

int64_t esp_timer_get_time() { return 1000000; }
using namespace esphome::openathan_device;
struct Audio : openathan::Playback {
  bool active{}; uint32_t epoch{}; std::string url;
  openathan::PlaybackSource kind{openathan::PlaybackSource::NONE};
  bool ready() const override { return true; }
  bool playing() const override { return active; }
  bool stream_supported() const override { return true; }
  uint32_t playback_epoch() const override { return epoch; }
  openathan::PlaybackSource source() const override { return kind; }
  bool start(openathan::Track) override { ++epoch; active = true; kind = openathan::PlaybackSource::ATHAN; return true; }
  void stop() override { ++epoch; active = false; kind = openathan::PlaybackSource::NONE; }
  bool start_stream(const std::string &value) override { url = value; active = true; kind = openathan::PlaybackSource::QURAN; return true; }
};
JsonDocument input(const char *text) { JsonDocument doc; assert(!deserializeJson(doc, text)); return doc; }
void execute(Quran &service) { run_worker(); service.loop(true, false); assert(live_http_clients == 0); }
int main() {
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
  Quran service; service.begin(&athan); service.loop(true, false);
  const std::string base = "https://www.mp3quran.net/api/v3/";
  const std::string entry = R"({"id":1,"name":"Example","moshaf":[{"id":9,"name":"Edition","server":"https://cdn.mp3quran.net/example/","surah_list":"1,18,114"}]})";
  http_replies[base + "reciters?language=eng&reciter=1"] = {200, "", "{\"reciters\":[" + entry + "]}"};
  std::string error;
  auto play = input(R"({"reciter":1,"edition":9,"surah":18})");
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 200);
  athan.stop(); execute(service); assert(audio.url.empty()); // Stop while resolving.
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 200);
  audio.start(openathan::Track::NORMAL); execute(service); assert(audio.url.empty()); // Athan beats late result.
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  audio.stop();
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 200);
  execute(service); assert(audio.url == "https://cdn.mp3quran.net/example/018.mp3");
  audio.stop(); audio.url.clear();
  auto unavailable = input(R"({"reciter":1,"edition":9,"surah":2})");
  assert(service.action("play", unavailable.as<JsonObjectConst>(), error) == 200); execute(service);
  JsonDocument state; service.snapshot(state.to<JsonObject>()); assert(state["state"] == "error" && audio.url.empty());
  service.loop(true, true); assert(service.action("play", play.as<JsonObjectConst>(), error) == 409);
  service.loop(false, false); assert(service.action("play", play.as<JsonObjectConst>(), error) == 503);
  service.loop(true, false);
  task_available = false;
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 503); task_available = true;
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
  assert(service.action("catalog", catalog.as<JsonObjectConst>(), error) == 200); execute(service);
  state.clear(); service.catalog_snapshot(state.to<JsonObject>());
  assert(state["data"]["total"] == 1 && state["data"]["reciters"][0]["id"] == 25);
  // Redirect destination is rejected before opening its connection.
  reset_http();
  http_replies[base + "reciters?language=eng&reciter=1"] = {302, "https://evil.example/reciters", ""};
  assert(service.action("play", play.as<JsonObjectConst>(), error) == 200); execute(service);
  assert(http_requests.size() == 1 && last_bundle && http_requests[0].auto_redirect_disabled);
  reset_http();
  const std::string audio_url = "https://cdn.mp3quran.net/example/018.mp3";
  const std::string redirected = "https://server15.mp3quran.net/example/018.mp3";
  http_replies[audio_url] = {302, redirected, ""};
  http_replies[redirected] = {200, "", "mp3"};
  { QuranHttp response; assert(response.open(audio_url, true)); }
  assert(http_requests.size() == 2 && live_http_clients == 0);
  reset_http();
  for (const auto &destination : {"http://cdn.mp3quran.net/example/018.mp3", "https://evil.example/018.mp3"}) {
    http_replies[audio_url] = {302, destination, ""};
    { QuranHttp response; assert(!response.open(audio_url, true)); }
    assert(http_requests.back().url == audio_url && live_http_clients == 0);
  }
  std::cout << "Quran parser, URL bounds, cancellation, priority, availability and HTTPS policy passed\n";
}

#include "quran.h"
#include "../openathan_audio/quran_policy.h"
#include "../openathan_audio/quran_http.h"
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <array>
#include <cstdio>
#include <functional>
#include <new>

namespace esphome::openathan_device {
namespace {
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
bool objects(const std::string &url, const std::string &key, std::atomic<bool> &cancel,
             const std::function<bool(JsonObjectConst)> &consume) {
  const auto deadline = now_ms() + 30000;
  QuranHttp reader;
  if (cancel || !reader.open(url)) return false;
  QuranObjectStream parser(key);
  std::array<char, 1024> buffer{};
  std::string object;
  while (!cancel && now_ms() < deadline) {
    const auto size = esp_http_client_read(reader.client, buffer.data(), buffer.size());
    if (size < 0) return false;
    if (size == 0) return esp_http_client_is_complete_data_received(reader.client) && parser.complete();
    for (int i = 0; i < size; ++i) {
      if (!parser.feed(buffer[i], object)) return false;
      if (!object.empty()) {
        JsonDocument entry;
        if (deserializeJson(entry, object) || !entry.is<JsonObject>() || !consume(entry.as<JsonObjectConst>())) return false;
      }
    }
  }
  return false;
}
bool name(JsonVariantConst value) {
  return value.is<const char *>() && !value.as<std::string>().empty() && value.as<std::string>().size() <= 256;
}
bool surahs(const std::string &text, JsonArray target) {
  if (text.empty() || text.size() > 512) return false;
  std::array<bool, 115> seen{};
  unsigned n = 0; bool digits = false;
  for (size_t i = 0; i <= text.size(); ++i) {
    const char c = i == text.size() ? ',' : text[i];
    if (c >= '0' && c <= '9') { n = n * 10 + c - '0'; digits = true; if (n > 114) return false; }
    else if (c == ',') {
      if (!digits || !n || seen[n]) return false;
      seen[n] = true; target.add(n); n = 0; digits = false;
    } else return false;
  }
  return true;
}
}
void Quran::fetch_(Job &job) {
  JsonDocument output;
  auto root = output.to<JsonObject>();
  bool ok = true;
  if (job.kind == "reciters") {
    auto list = root["reciters"].to<JsonArray>();
    unsigned count = 0, scanned = 0;
    ok = objects("https://www.mp3quran.net/api/v3/reciters?language=eng", "reciters", job.cancel,
      [&](JsonObjectConst entry) {
        if (++scanned > 1024 || !entry["id"].is<unsigned>() || !entry["id"].as<unsigned>() || !name(entry["name"])) return false;
        auto value = entry["name"].as<std::string>();
        for (auto &c : value) c = std::tolower(static_cast<unsigned char>(c));
        if (value.find(job.query) == std::string::npos) return true;
        ++count;
        if (count > job.offset && count <= job.offset + 20) {
          auto row = list.add<JsonObject>(); row["id"] = entry["id"]; row["name"] = entry["name"];
        }
        return true;
      });
    root["total"] = count; root["offset"] = job.offset;
    if (job.offset + 20 < count) root["next_offset"] = job.offset + 20;
    else root["next_offset"] = nullptr;
    auto chapters = root["surahs"].to<JsonArray>();
    std::array<bool, 115> seen{};
    if (ok) ok = objects("https://www.mp3quran.net/api/v3/suwar?language=eng", "suwar", job.cancel,
      [&](JsonObjectConst entry) {
        const auto id = entry["id"].as<unsigned>();
        if (!entry["id"].is<unsigned>() || !id || id > 114 || seen[id] || !name(entry["name"])) return false;
        seen[id] = true;
        auto row = chapters.add<JsonObject>(); row["id"] = id; row["name"] = entry["name"]; return true;
      });
    ok = ok && chapters.size() == 114;
  } else {
    bool found = false;
    auto editions = root["editions"].to<JsonArray>();
    ok = objects("https://www.mp3quran.net/api/v3/reciters?language=eng&reciter=" + std::to_string(job.reciter),
      "reciters", job.cancel, [&](JsonObjectConst entry) {
        if (found || !entry["id"].is<unsigned>() || entry["id"].as<unsigned>() != job.reciter ||
            !entry["moshaf"].is<JsonArrayConst>() || entry["moshaf"].size() > 32) return false;
        found = true;
        for (auto edition : entry["moshaf"].as<JsonArrayConst>()) {
          if (!edition["id"].is<unsigned>() || !edition["id"].as<unsigned>() || !name(edition["name"]) ||
              !edition["surah_list"].is<const char *>() || !edition["server"].is<const char *>()) return false;
          JsonDocument numbers;
          auto available = numbers.to<JsonArray>();
          if (!surahs(edition["surah_list"].as<std::string>(), available)) return false;
          if (job.kind == "play") {
            if (edition["id"].as<unsigned>() != job.edition) continue;
            bool listed = false;
            for (auto id : available) if (id.as<unsigned>() == job.surah) listed = true;
            if (!listed || !job.url.empty()) return false;
            const auto server = edition["server"].as<std::string>();
            if (!quran_audio_url(server) || server.back() != '/') return false;
            char file[8]; std::snprintf(file, sizeof(file), "%03u.mp3", job.surah);
            job.url = server + file;
          } else {
            auto row = editions.add<JsonObject>(); row["id"] = edition["id"]; row["name"] = edition["name"];
            row["surahs"].set(available);
          }
        }
        return true;
      });
    ok = ok && found && (job.kind != "play" || !job.url.empty());
  }
  if (!ok) { job.error = "MP3Quran is unavailable or returned an unsupported recording. Try again."; return; }
  if (measureJson(output) > 32768) { job.error = "MP3Quran catalog exceeds the device limit"; return; }
  serializeJson(output, job.response);
}
void Quran::worker_(void *context) {
  std::unique_ptr<std::pair<Quran *, std::shared_ptr<Job>>> work(
      static_cast<std::pair<Quran *, std::shared_ptr<Job>> *>(context));
  work->first->fetch_(*work->second);
  work->second->done = true;
  work.reset();
  vTaskDelete(nullptr);
}
int Quran::action(const std::string &action, JsonObjectConst input, std::string &error) {
  auto *playback = athan_ ? athan_->playback() : nullptr;
  if (!playback || !playback->stream_supported()) { error = "Quran streaming is unavailable"; return 503; }
  if (!connected_ || !athan_->read().valid) { error = "Connect Wi-Fi and wait for the clock before streaming"; return 503; }
  if (!athan_->activated() || !athan_->upgrade_health() || !athan_->settings_service() || !athan_->settings_service()->healthy()) {
    error = "Finish setup and resolve device faults before streaming"; return 409;
  }
  if (blocked_ || job_ || playback->source() == ::openathan::PlaybackSource::ATHAN) {
    error = "Wait for Athan, the current request or firmware maintenance to finish"; return 409;
  }
  auto job = std::make_shared<Job>();
  if (action == "play") {
    if (input.size() != 3 || !input["reciter"].is<unsigned>() || !input["edition"].is<unsigned>() ||
        !input["surah"].is<unsigned>() || !input["reciter"].as<unsigned>() || !input["edition"].as<unsigned>() ||
        !input["surah"].as<unsigned>() || input["surah"].as<unsigned>() > 114) { error = "Select a reciter, edition and surah"; return 400; }
    job->kind = "play"; job->reciter = input["reciter"]; job->edition = input["edition"]; job->surah = input["surah"];
  } else {
    const auto kind = input["kind"].as<std::string>();
    if ((kind == "reciters" ? input.size() != 3 : input.size() != 2) || (kind != "reciters" && kind != "editions") ||
        (kind == "reciters" && (!input["query"].is<const char *>() || input["query"].as<std::string>().size() > 128)) ||
        (kind == "reciters" && (!input["offset"].is<unsigned>() || input["offset"].as<unsigned>() > 1024)) ||
        (kind == "editions" && (!input["reciter"].is<unsigned>() || !input["reciter"].as<unsigned>()))) {
      error = "Invalid catalog request"; return 400;
    }
    job->kind = kind; job->offset = input["offset"] | 0U; job->reciter = input["reciter"] | 0U;
    job->query = input["query"] | std::string{};
    for (auto &c : job->query) c = std::tolower(static_cast<unsigned char>(c));
  }
  auto *context = new (std::nothrow) std::pair<Quran *, std::shared_ptr<Job>>(this, job);
  if (!context) { error = "Not enough memory to stream"; return 503; }
  // Only a play request replaces Quran; catalog browsing does not alter audio.
  if (action == "play") athan_->stop();
  job->epoch = playback->playback_epoch(); job->request = ++request_;
  job_ = job;
  if (xTaskCreate(worker_, "oa_quran", 8192, context, 3, nullptr) != pdPASS) {
    delete context; job_.reset(); error = "Not enough memory to stream"; return 503;
  }
  if (action == "play") {
    resolving_ = true; playback_error_.clear(); reciter_ = job->reciter; edition_ = job->edition; surah_ = job->surah;
  } else {
    catalog_state_ = "loading"; catalog_request_ = job->request; catalog_response_.clear(); catalog_error_.clear();
  }
  return 200;
}
void Quran::loop(bool connected, bool blocked) {
  connected_ = connected; blocked_ = blocked;
  auto *playback = athan_ ? athan_->playback() : nullptr;
  if (!playback) return;
  if (job_ && (!connected || blocked || job_->epoch != playback->playback_epoch())) job_->cancel = true;
  if ((!connected || !playback->ready()) && playback->source() == ::openathan::PlaybackSource::QURAN) {
    athan_->stop(); playback_error_ = !connected ? "Wi-Fi disconnected. Press Play after reconnecting." : "The speaker is unavailable. Check the device before retrying.";
  }
  if (blocked && (resolving_ || playback->source() == ::openathan::PlaybackSource::QURAN)) athan_->stop();
  if (job_ && job_->cancel && job_->kind == "play") resolving_ = false;
  if (!job_ || !job_->done) return;
  auto job = std::move(job_);
  if (job->kind == "play") {
    resolving_ = false;
    if (!job->cancel) {
      if (!job->error.empty()) playback_error_ = job->error;
      else if (!playback->start_stream(job->url)) playback_error_ = "The speaker could not start Quran playback";
    }
  } else {
    catalog_state_ = !job->cancel && job->error.empty() ? "ready" : "error";
    catalog_error_ = job->cancel ? "Catalog request interrupted. Try again." : job->error;
    catalog_response_ = job->cancel ? "" : std::move(job->response);
  }
}
void Quran::shutdown() { if (job_) job_->cancel = true; resolving_ = false; }
void Quran::snapshot(JsonObject root) {
  auto *playback = athan_ ? athan_->playback() : nullptr;
  root["supported"] = playback && playback->stream_supported();
  const auto state = playback ? playback->stream_state() : ::openathan::StreamState::IDLE;
  const bool pending = resolving_ && job_ && playback && job_->epoch == playback->playback_epoch();
  root["state"] = pending ? "loading" : !playback_error_.empty() || state == ::openathan::StreamState::ERROR ? "error" :
      state == ::openathan::StreamState::PLAYING ? "playing" : state == ::openathan::StreamState::LOADING ? "loading" : "idle";
  root["error"] = state == ::openathan::StreamState::ERROR ? "The recording could not be played. Try another recording or retry." : playback_error_;
  root["reciter"] = reciter_; root["edition"] = edition_; root["surah"] = surah_;
}
void Quran::catalog_snapshot(JsonObject root) {
  root["state"] = catalog_state_; root["request"] = catalog_request_; root["error"] = catalog_error_;
  if (!catalog_response_.empty()) {
    JsonDocument document;
    if (!deserializeJson(document, catalog_response_)) root["data"].set(document.as<JsonObjectConst>());
  }
}
}

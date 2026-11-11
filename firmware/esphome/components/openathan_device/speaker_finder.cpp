#include "speaker_finder.h"

#include <algorithm>
#include <utility>

#include "openathan/speaker_name.h"

namespace esphome::openathan_device::speaker_finder {
namespace {
constexpr std::string_view prefix = "openathan-";

void append_text(std::string &html, std::string_view text) {
  for (char character : text) {
    switch (character) {
      case '&': html += "&amp;"; break;
      case '<': html += "&lt;"; break;
      case '>': html += "&gt;"; break;
      default: html += character; break;
    }
  }
}
}  // namespace

bool valid_hostname(std::string_view hostname) {
  if (!hostname.starts_with(prefix) || hostname.size() != prefix.size() + 6) return false;
  return std::all_of(hostname.begin() + prefix.size(), hostname.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

void record(std::vector<Speaker> &speakers, Speaker speaker) {
  if (!valid_hostname(speaker.hostname)) return;
  std::string name;
  if (!::openathan::normalize_speaker_name(speaker.name, name)) name.clear();
  speaker.name = std::move(name);
  auto existing = std::lower_bound(speakers.begin(), speakers.end(), speaker.hostname,
      [](const Speaker &peer, std::string_view hostname) { return peer.hostname < hostname; });
  if (existing != speakers.end() && existing->hostname == speaker.hostname) {
    if (speaker.candidate && !existing->candidate) *existing = std::move(speaker);
    else existing->owner |= speaker.owner;
    return;
  }
  if (speakers.size() < max_speakers) speakers.insert(existing, std::move(speaker));
}

Action Election::observe(std::string_view self, bool owner, const std::vector<Speaker> &peers) {
  const auto better = std::any_of(peers.begin(), peers.end(), [&](const Speaker &peer) {
    if (!peer.candidate || peer.hostname == self) return false;
    return owner ? peer.owner && peer.hostname < self : peer.owner || peer.hostname < self;
  });
  if (better || owner) {
    quiet_rounds_ = 0;
    return better && owner ? Action::YIELD : Action::WAIT;
  }
  if (quiet_rounds_ < 2) ++quiet_rounds_;
  return quiet_rounds_ == 2 ? Action::CHECK_ALIAS : Action::WAIT;
}

std::string page(const std::vector<Speaker> &speakers) {
  std::string html = R"(<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Find my OpenAthan speaker</title><link rel="stylesheet" href="/finder.css">
<main><h1>Find my speaker</h1><p>Choose an OpenAthan speaker on your home Wi-Fi.</p><ul>)";
  for (const auto &speaker : speakers) {
    if (!valid_hostname(speaker.hostname)) continue;
    html += "<li><a href=\"http://";
    html += speaker.hostname;
    html += ".local/\"><bdi>";
    append_text(html, speaker.name.empty() ? std::string_view("OpenAthan") : speaker.name);
    html += "</bdi><small>";
    html += std::string_view(speaker.hostname).substr(prefix.size());
    html += "</small></a></li>";
  }
  html += "</ul><p><a href=\"/\">Refresh list</a></p></main></html>";
  return html;
}
}  // namespace esphome::openathan_device::speaker_finder

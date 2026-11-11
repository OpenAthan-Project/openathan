#include "speaker_finder.h"

#include <cstdio>
#include <cstdlib>

namespace finder = esphome::openathan_device::speaker_finder;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)

int main() {
  const std::string self = "openathan-123abc";
  const std::string lower = "openathan-000001";
  const std::string higher = "openathan-ffffff";
  finder::Election election;
  std::vector<finder::Speaker> peers{{lower, ""}};
  CHECK(election.observe(self, false, peers) == finder::Action::WAIT);
  CHECK(election.observe(self, false, peers) == finder::Action::CHECK_ALIAS);
  peers = {{higher, "", true, true}};
  CHECK(election.observe(self, false, peers) == finder::Action::WAIT);
  peers.clear();
  CHECK(election.observe(self, false, peers) == finder::Action::WAIT);
  CHECK(election.observe(self, false, peers) == finder::Action::CHECK_ALIAS);
  peers = {{lower, "", true}};
  CHECK(election.observe(self, true, peers) == finder::Action::WAIT);
  CHECK(election.observe(self, false, peers) == finder::Action::WAIT);
  peers.front().owner = true;
  CHECK(election.observe(self, true, peers) == finder::Action::YIELD);
  peers.front().hostname = higher;
  CHECK(election.observe(self, true, peers) == finder::Action::WAIT);

  peers.clear();
  finder::record(peers, {self, ""});
  finder::record(peers, {self, "<Kitchen & dining>", true});
  finder::record(peers, {self, ""});
  finder::record(peers, {higher, "<Kitchen & dining>", true});
  finder::record(peers, {lower, std::string("a\0b", 3), true});
  finder::record(peers, {"openathan-test-123abc", ""});
  finder::record(peers, {"openathan-123abc.local", ""});
  finder::record(peers, {"other-speaker", ""});
  CHECK(peers.size() == 3);
  CHECK(peers[0].hostname == lower && peers[0].name.empty());
  const auto html = finder::page(peers);
  CHECK(html.find("http://openathan-123abc.local/") != std::string::npos);
  CHECK(html.find("http://openathan-ffffff.local/") != std::string::npos);
  CHECK(html.find("<bdi>OpenAthan</bdi><small>000001</small>") != std::string::npos);
  CHECK(html.find("&lt;Kitchen &amp; dining&gt;") != std::string::npos);
  CHECK(html.find("<Kitchen") == std::string::npos);
  CHECK(html.find("openathan-test-") == std::string::npos);
  for (unsigned i = 16; i < 50; ++i)
    finder::record(peers, {"openathan-0000" + std::to_string(i), ""});
  CHECK(peers.size() == finder::max_speakers);
}

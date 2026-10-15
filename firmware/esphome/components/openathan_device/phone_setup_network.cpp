#include "device.h"
#include "esphome/core/application.h"
#include <esp_netif.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <unistd.h>
#include <array>
#include <cstring>

namespace esphome::openathan_device {
bool Device::phone_eligible_() const {
  const auto *settings = athan_ ? athan_->settings_service() : nullptr;
  return phone_setup::eligible(athan_ && std::strcmp(athan_->setup_state(), "incomplete") == 0,
      store_.healthy() && settings && settings->healthy() && !maintenance_ && !upgrade_.streaming_blocked(),
      wifi_record_ == ::openathan::LoadResult::EMPTY, password_record_ == ::openathan::LoadResult::ERROR);
}
bool Device::phone_setup_active() const {
  const auto *wifi = wifi::global_wifi_component;
  return phone_session_ && wifi && (wifi->is_ap_active() || phone_state_ == phone_setup::State::CONNECTING);
}
void Device::phone_stop_dns_() {
  if (phone_dns_socket_ >= 0) { close(phone_dns_socket_); phone_dns_socket_ = -1; }
}
void Device::phone_dns_() {
  const auto *wifi = wifi::global_wifi_component;
  if (!phone_setup_active() || !wifi || !wifi->is_ap_active() || wifi_attempt_.active() || connected() ||
      !ipv4_address().empty()) { phone_stop_dns_(); return; }
  if (phone_dns_socket_ < 0) {
    auto *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!netif) return;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return;
    struct ifreq interface{};
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(53); address.sin_addr.s_addr = inet_addr("192.168.4.1");
    // Bind both the address and netif: station-side DNS must never answer setup
    // queries, even on a home network using the same subnet.
    if (esp_netif_get_netif_impl_name(netif, interface.ifr_name) != ESP_OK ||
        setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof(interface)) != 0 ||
        bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        fcntl(fd, F_SETFL, O_NONBLOCK) != 0) { close(fd); return; }
    phone_dns_socket_ = fd;
  }
  std::array<uint8_t, 512> query{}, answer{};
  sockaddr_in peer{}; socklen_t size = sizeof(peer);
  const auto bytes = recvfrom(phone_dns_socket_, query.data(), query.size(), MSG_DONTWAIT,
                              reinterpret_cast<sockaddr *>(&peer), &size);
  if (bytes <= 0) return;
  const auto length = phone_setup::dns_response(std::span(query.data(), static_cast<size_t>(bytes)), answer);
  if (length) sendto(phone_dns_socket_, answer.data(), length, MSG_DONTWAIT,
                     reinterpret_cast<sockaddr *>(&peer), size);
}
void Device::phone_loop_(uint64_t now) {
  if (!phone_initialized_ && App.is_setup_complete() && !wifi_attempt_.active() && !scanning_ &&
      !upgrade_.usb_busy()) {
    phone_initialized_ = true;
    if (phone_eligible_()) {
      phone_session_ = true;
      phone_state_ = phone_setup::State::READY;
      restore_wifi_();
    }
  }
  if (phone_session_ && (maintenance_ || upgrade_.streaming_blocked() ||
      std::strcmp(athan_->setup_state(), "incomplete") != 0 || wifi_record_ == ::openathan::LoadResult::LOADED)) {
    phone_session_ = false; phone_state_ = phone_setup::State::CLOSED;
    phone_candidate_ = {}; phone_response_.reset();
    if (wifi::global_wifi_component->is_ap_active()) wifi::global_wifi_component->disable();
  }
  if (phone_response_) {
    const unsigned result = phone_response_->response_result.load();
    if (result == 1 && phone_eligible_()) {
      phone_attempt_ = wifi_attempt_.begin(std::move(phone_candidate_), now);
      if (!phone_attempt_) { phone_state_ = phone_setup::State::FAILED; phone_error_ = "Could not start connection. Try again."; }
      else {
        // The handoff response is already sent. Close the enrollment network
        // before enabling STA, using the same disconnect-first path as USB.
        wifi::global_wifi_component->disable();
        phone_stop_dns_();
      }
      phone_response_.reset();
    } else if (result == 2 || now - phone_response_at_ >= 6000 || !phone_eligible_()) {
      phone_candidate_ = {}; phone_response_.reset();
      phone_state_ = phone_setup::State::FAILED;
      phone_error_ = "Connection request interrupted. Join setup Wi-Fi and try again.";
    }
  }
  phone_dns_();
}
}  // namespace esphome::openathan_device

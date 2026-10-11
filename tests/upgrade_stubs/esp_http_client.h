#pragma once
#include <string>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <map>
#include <vector>
using esp_err_t=int;
constexpr int HTTP_EVENT_ON_HEADER=1;
struct esp_http_client_event_t{int event_id;char *header_key,*header_value;void *user_data;};
struct esp_http_client_config_t{const char *url;const char *cert_pem;int(*crt_bundle_attach)(void *);int timeout_ms;bool disable_auto_redirect;int buffer_size,buffer_size_tx;int(*event_handler)(esp_http_client_event_t *);void *user_data;};
struct HttpReply{int status{200};std::string location,data;};
struct HttpRequest{std::string url;int tx,rx,timeout;bool auto_redirect_disabled;};
struct Http{std::string data;size_t offset{};HttpReply reply;esp_http_client_config_t config;std::string url;};
using esp_http_client_handle_t=Http *;
inline std::string descriptor_response,application_response;
inline bool transfer_fail=false;
inline void (*on_read)()=nullptr;
inline void (*on_open)()=nullptr;
inline std::string last_url,last_ca;
inline bool last_bundle{};
inline std::map<std::string,HttpReply> http_replies;
inline std::vector<HttpRequest> http_requests;
enum class HttpFailure{NONE,INIT,OPEN,HEADERS};
inline HttpFailure http_failure=HttpFailure::NONE;
inline std::string http_failure_url;
inline unsigned live_http_clients{},http_cleanups{},live_http_tx_bytes{},peak_http_tx_bytes{};
inline void reset_http(){assert(live_http_clients==0 && live_http_tx_bytes==0);http_replies.clear();http_requests.clear();http_failure=HttpFailure::NONE;http_failure_url.clear();http_cleanups=peak_http_tx_bytes=0;}
inline bool http_fails(HttpFailure stage,const std::string &url){return http_failure==stage && http_failure_url==url;}
inline Http *esp_http_client_init(const esp_http_client_config_t *c){
  last_url=c->url;last_ca=c->cert_pem?c->cert_pem:"";last_bundle=c->crt_bundle_attach!=nullptr;
  http_requests.push_back({last_url,c->buffer_size_tx,c->buffer_size,c->timeout_ms,c->disable_auto_redirect});
  if(http_fails(HttpFailure::INIT,last_url))return nullptr;
  // Redirect clients must be released before allocating the next buffer.
  assert(live_http_clients==0);++live_http_clients;live_http_tx_bytes+=c->buffer_size_tx;
  peak_http_tx_bytes=std::max(peak_http_tx_bytes,live_http_tx_bytes);
  const auto found=http_replies.find(last_url);
  auto reply=found==http_replies.end()?HttpReply{200,"",last_url.ends_with("upgrade.json")?descriptor_response:application_response}:found->second;
  return new Http{reply.data,0,reply,*c,last_url};
}
inline int esp_http_client_cleanup(Http *h){--live_http_clients;live_http_tx_bytes-=h->config.buffer_size_tx;++http_cleanups;delete h;return 0;}
inline int esp_http_client_open(Http *h,int){
  if(on_open){auto callback=on_open;on_open=nullptr;callback();}
  if(http_fails(HttpFailure::OPEN,h->url))return -1;
  const auto path=h->url.find('/',8);
  const auto target=path==std::string::npos?"/":h->url.substr(path);
  // IDF needs a NUL and room for at least its first default header. The actual
  // pinned formatter/header sender is exercised by check_upgrade_http.py.
  const auto required=std::string("GET ").size()+target.size()+std::string(" HTTP/1.1\r\nUser-Agent: ESP32 HTTP Client/1.0\r\n").size()+3;
  return required>static_cast<size_t>(h->config.buffer_size_tx)?-1:0;
}
inline int esp_http_client_fetch_headers(Http *h){
  if(http_fails(HttpFailure::HEADERS,h->url))return -1;
  if(!h->reply.location.empty()){
    char key[]="Location";
    esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,key,h->reply.location.data(),h->config.user_data};
    h->config.event_handler(&event);
  }
  return h->data.size();
}
inline int esp_http_client_get_status_code(Http *h){return h->reply.status;}
inline int esp_http_client_set_user_data(Http *h,void *value){h->config.user_data=value;return 0;}
inline int esp_http_client_get_content_length(Http *h){return h->data.size();}
inline int esp_http_client_read(Http *h,char *out,int size){if(on_read){auto callback=on_read;on_read=nullptr;callback();}if(transfer_fail)return -1;size=std::min<size_t>(size,h->data.size()-h->offset);memcpy(out,h->data.data()+h->offset,size);h->offset+=size;return size;}
inline bool esp_http_client_is_complete_data_received(Http *h){return h->offset==h->data.size();}

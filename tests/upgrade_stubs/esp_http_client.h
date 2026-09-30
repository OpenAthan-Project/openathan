#pragma once
#include <string>
#include <algorithm>
#include <cstring>
using esp_err_t=int;
constexpr int HTTP_EVENT_ON_HEADER=1;
struct esp_http_client_event_t{int event_id;char *header_key,*header_value;void *user_data;};
struct esp_http_client_config_t{const char *url;int(*crt_bundle_attach)(void *);int timeout_ms;bool disable_auto_redirect;int buffer_size,buffer_size_tx;int(*event_handler)(esp_http_client_event_t *);void *user_data;};
struct Http{std::string data;size_t offset{};};
using esp_http_client_handle_t=Http *;
inline std::string descriptor_response,application_response;
inline bool transfer_fail=false;
inline void (*on_read)()=nullptr;
inline void (*on_open)()=nullptr;
inline Http *esp_http_client_init(const esp_http_client_config_t *c){return new Http{std::string(c->url).ends_with("upgrade.json")?descriptor_response:application_response};}
inline int esp_http_client_cleanup(Http *h){delete h;return 0;}
inline int esp_http_client_open(Http *,int){if(on_open){auto callback=on_open;on_open=nullptr;callback();}return 0;}
inline int esp_http_client_fetch_headers(Http *h){return h->data.size();}
inline int esp_http_client_get_status_code(Http *){return 200;}
inline int esp_http_client_get_content_length(Http *h){return h->data.size();}
inline int esp_http_client_read(Http *h,char *out,int size){if(on_read){auto callback=on_read;on_read=nullptr;callback();}if(transfer_fail)return -1;size=std::min<size_t>(size,h->data.size()-h->offset);memcpy(out,h->data.data()+h->offset,size);h->offset+=size;return size;}
inline bool esp_http_client_is_complete_data_received(Http *h){return h->offset==h->data.size();}

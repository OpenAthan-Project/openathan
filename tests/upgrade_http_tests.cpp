// Compile the resolved SDK functions unchanged. Only transport/events and the
// client fields needed by those functions are adapted for this host regression.
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "http_header.h"
#include "esp_log.h"
#include "idf_defaults.h"
constexpr int HTTP_METHOD_GET=0,HTTP_METHOD_HEAD=1,HTTP_METHOD_DELETE=2;
constexpr int HTTP_STATE_REQ_COMPLETE_HEADER=1,HTTP_EVENT_HEADERS_SENT=1,ESP_ERR_HTTP_WRITE_DATA=-2;
const char *HTTP_METHOD_MAPPING[]={"GET","HEAD","DELETE"};
const char *DEFAULT_HTTP_PROTOCOL="HTTP/1.1";
struct Buffer{char *data;};
struct Request{Buffer *buffer;http_header_handle_t headers;};
struct Connection{int method;const char *path,*query;};
struct Client{
  int buffer_size_tx;Request *request;Connection connection_info;
  bool first_line_prepared{},is_async{};
  int header_index{},data_written_index{},data_write_left{},post_len{},state{},timeout_ms{4000};
  void *transport{};
};
using esp_http_client_handle_t=Client *;
std::string sent;
bool write_fails{};
unsigned closes{};
int esp_http_client_set_header(Client *client,const char *key,const char *value){return http_header_set(client->request->headers,key,value);}
int esp_transport_write(void *,const char *data,int size,int){if(write_fails)return -1;sent.append(data,size);return size;}
int esp_http_client_write(Client *,const char *data,int size){return esp_transport_write(nullptr,data,size,0);}
void esp_http_client_close(Client *){++closes;}
void http_dispatch_event(Client *,int,void *,int){}
void http_dispatch_event_to_event_loop(int,void *,size_t){}
#include "idf_request.inc"

bool send(const std::string &url,int capacity,bool fail=false) {
  const auto slash=url.find('/',8);assert(slash!=std::string::npos);
  const auto host=url.substr(8,slash-8);
  const auto question=url.find('?',slash);
  const auto path=url.substr(slash,question==std::string::npos?std::string::npos:question-slash);
  const auto query=question==std::string::npos?"":url.substr(question+1);
  auto headers=http_header_init();assert(headers);
  assert(http_header_set(headers,"User-Agent",USER_AGENT)==ESP_OK);
  assert(http_header_set(headers,"Host",host.c_str())==ESP_OK);
  // The GET formatter must remove this initial length header.
  assert(http_header_set(headers,"Content-Length","0")==ESP_OK);
  std::vector<char> buffer(capacity);Buffer b{buffer.data()};Request request{&b,headers};
  Client client{capacity,&request,{HTTP_METHOD_GET,path.c_str(),question==std::string::npos?nullptr:query.c_str()}};
  sent.clear();write_fails=fail;closes=0;
  const auto result=esp_http_client_request_send(&client,0);
  const auto expected="GET "+url.substr(slash)+" HTTP/1.1\r\nUser-Agent: "+USER_AGENT+"\r\nHost: "+host+"\r\n\r\n";
  assert(http_header_destroy(headers)==ESP_OK);
  if(fail)assert(result==ESP_ERR_HTTP_WRITE_DATA && closes==1 && sent.empty());
  return result==ESP_OK && sent==expected;
}

int main() {
  for(const auto *host:{"github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"}) {
    const std::string prefix=std::string("https://")+host+"/";
    const auto short_url=prefix+"upgrade.json";
    assert(send(short_url,short_url.size()+512));
    for(const auto size:{size_t{905},size_t{4096}}) {
      const auto url=prefix+"?sig="+std::string(size-prefix.size()-5,'q');
      assert(url.size()==size);
      assert(!send(url,512));
      assert(send(url,url.size()+512));
      assert(!send(url,url.size()+512,true));
    }
    const auto path=prefix+std::string(4096-prefix.size(),'p');
    assert(send(path,path.size()+512));
  }
  // A 4096-byte fixed buffer can fit the maximum GitHub request line but has
  // only three bytes left. IDF emits no request if the first header cannot fit.
  const std::string prefix="https://github.com/?q=";
  const auto boundary=prefix+std::string(4096-prefix.size(),'q');
  assert(!send(boundary,4096) && sent.empty());
  assert(send(boundary,4608));
  // Headers can also continue in a second write after the first header fits.
  const std::string short_url="https://github.com/upgrade.json";
  const auto line=std::string("GET /upgrade.json HTTP/1.1\r\n");
  const auto first=std::string("User-Agent: ")+USER_AGENT+"\r\n";
  assert(send(short_url,line.size()+first.size()+3));
  std::cout<<"Pinned ESP-IDF request-line and header transmission regressions passed\n";
}

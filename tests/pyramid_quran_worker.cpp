// Metadata-worker controls compiled with the production Quran host environment.
#include <esp_http_client.h>
#include <freertos/task.h>
#include <cassert>

void prepare_volume_quran_metadata() {
  http_replies["https://www.mp3quran.net/api/v3/reciters?language=eng&reciter=1"] =
      {200, "", R"({"reciters":[{"id":1,"name":"Example","moshaf":[{"id":9,"name":"Edition","server":"https://cdn.mp3quran.net/example/","surah_list":"18"}]}]})"};
}
void finish_volume_quran_metadata() {
  run_worker();
  assert(live_http_clients == 0);
}

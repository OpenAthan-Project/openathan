#define main production_fixture_main
#include "upgrade_runtime_tests.cpp"
#undef main
int main(){
  reset();Upgrade u;esphome::openathan_component::OpenAthan a;begin(u,a);
  now_us=60000000;u.loop(true);assert(!pending_worker&&task_attempts==0);
  offer(u);assert(action(u,"install","v0.3.0")==503);
  u.loop(true);assert(erases==0&&boot_selections==0);
  std::cout<<"Ordinary isolated firmware rejects installation and automatic release checks\n";
}

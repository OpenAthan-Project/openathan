#pragma once
#include "fake_i2s.h"
namespace esphome {
template<class T>using optional=std::optional<T>;
template<class T>class Parented{public:void set_parent(T*p){parent_=p;}protected:T*parent_{};};
struct Mutex {
 bool locked{};
 void lock(){assert(!locked);locked=true;}
 bool try_lock(){if(locked)return false;locked=true;return true;}
 void unlock(){assert(locked);locked=false;}
};
template<class T>struct RAMAllocator{T*allocate(size_t n){return fake_i2s::allocation_failure?nullptr:new T[n];}void deallocate(T*p,size_t){delete[]p;}};
template<class O,class I>O remap(I x,I lo,I hi,O a,O b){return a+(x-lo)*(b-a)/(hi-lo);}
}


#ifndef __GLOBAL_MSG_H__
#define __GLOBAL_MSG_H__

#include "arch/x86/faults.hh"
#include <map>

/*
class global_msg
{
  public:
    static global_msg *Instance();
    std::map<uint64_t, stacklet_message_t> msg_map;
    uint64_t msg_counter;
  private:
    global_msg() {};
    static global_msg *singleton_map;
};

//global_msg::singleton_map = NULL;
global_msg *global_msg::Instance()
{
  if(!singleton_map)
  {
    singleton_map = new global_msg();
    singleton_map->msg_counter = 0;
  }

  return singleton_map;
}


*/
std::map<uint64_t, stacklet_message_t> msg_map;
uint64_t msg_counter;

std::map<uint64_t, uint64_t> cr3_map;
#endif


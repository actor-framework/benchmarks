#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include "charm++.h"
#include "mailbox.decl.h"

struct main : public CBase_main {
  void usage() {
    using namespace std;
    cout << endl
         << "./mailbox NUM_THREADS MSG_PER_THREAD" << endl
         << endl << endl;
  }   
   
  main(CkArgMsg* m) {
    using namespace std;
    if (m->argc != 3) {
      usage();
      CkExit();
    }   
    int s_num = atoi(m->argv[1]);
    try {
      run(atoi(m->argv[1]), atoi(m->argv[2]));
    } catch (std::exception&) {
      usage();
      CkExit();
    }
  }   

  void run(uint64_t num_sender, uint64_t num_msgs) {
    uint64_t total = num_sender * num_msgs;
    CProxy_receiver testee = CProxy_receiver::ckNew(total);
    for (uint64_t i = 0; i < num_sender; ++i) {
      CProxy_sender::ckNew(testee, num_msgs);
    }
  }
};

class sender : public CBase_sender {
 public:
   
  sender(CProxy_receiver receiver, uint64_t count) {
      for (uint64_t i = 0; i < count; ++i) {
        receiver.receive();
      }     
  }
};

class receiver : public CBase_receiver {
  receiver_SDAG_CODE

  int m_max;
  int m_value;
public:
  
  receiver(uint64_t max) :
  m_max(max), m_value(0) {
    behavior();    
  }
};

#include "mailbox.def.h"

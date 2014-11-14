#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "charm++.h"
#include "charm_mailbox_performance.decl.h"

struct main : public CBase_main {
  main(CkArgMsg* m) {
    if (m->argc != 3) {
      std::cout << std::endl
                << "./charm_mailbox_performance NUM_THREADS MSG_PER_THREAD"
                << std::endl << std::endl;
      CkExit();
    }
    int num_sender = atoi(m->argv[1]);
    int num_msgs = atoi(m->argv[2]);
    delete m;
    uint64_t total = static_cast<uint64_t>(num_sender) * num_msgs;
    CProxy_receiver testee = CProxy_receiver::ckNew(total);
    for (int i = 0; i < num_sender; ++i) {
      CProxy_sender s = CProxy_sender::ckNew(testee, num_msgs);
      s.run();
    }
  }
};

class sender : public CBase_sender {
 public:
  sender(CProxy_receiver receiver, int count)
      : m_receiver(receiver), m_count(count) {
    // nop
    //CkPrintf("%s", "sender::sender\n");
  }

  void run() {
    for (int i = 0; i < m_count; ++i) {
      m_receiver.msg(0); // dummy value
    }
    //CkPrintf("%s", "sender::run() done\n");
    delete this;
  }

 private:
  CProxy_receiver m_receiver;
  int m_count;
};

class receiver : public CBase_receiver {
public:
  receiver(uint64_t max) : m_max(max), m_value(0) {
    // nop
    //CkPrintf("%s", "receiver::receiver");
  }

  void msg(int) {
    if (++m_value == m_max) {
      //CkPrintf("%s%i%s", "received ", m_max, " dummy messages\n");
      CkExit();
    }
  }

 private:
  int m_max;
  int m_value;
};

#include "charm_mailbox_performance.def.h"

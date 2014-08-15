#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "charm++.h"
#include "mailbox.decl.h"

struct main : public CBase_main {
  main(CkArgMsg* m) {
    int num_sender;
    int num_msgs;
    try {
      if (m->argc != 3) {
        throw std::runtime_error("invalid number of arguments");
      }
      num_sender = atoi(m->argv[1]);
      num_msgs = atoi(m->argv[2]);
    } catch (std::exception&) {
      std::cout << std::endl
                << "./charm_mailbox_performance NUM_THREADS MSG_PER_THREAD"
                << std::endl << std::endl;
      CkExit();
    }
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
  }

  void run() {
    for (int i = 0; i < m_count; ++i) {
      m_receiver.msg(0); // dummy value
    }
  }

 private:
  CProxy_receiver m_receiver;
  int m_count;
};

class receiver : public CBase_receiver {
public:
  receiver(uint64_t max) : m_max(max), m_value(0) {
    // nop
  }

  void msg(int) {
    if (++m_value == m_max) {
      std::cout << "received " << m_max << " dummy messages" << std::endl;
      CkExit();
    }
  }

 private:
  int m_max;
  int m_value;
};

#include "mailbox.def.h"

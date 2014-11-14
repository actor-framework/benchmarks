#include <iostream>
#include <stdexcept>

#include "charm++.h"
#include "charm_actor_creation.decl.h"

class main : public CBase_main {
 public:
  main(CkArgMsg* m) {
    int num;
    try {
      if (m->argc != 2) {
        throw std::runtime_error("invalid number of arguments");
      }
      num = atoi(m->argv[1]);
    }
    catch (std::exception&) {
      std::cout << std::endl << std::endl << std::endl << "./creation (1-9)*"
                << std::endl << " creates 2^POW actors" << std::endl
                << std::endl;
      CkExit();
    }
    CProxy_testee root = CProxy_testee::ckNew(true, CProxy_testee());
    root.spread(num);
  }
};

class testee : public CBase_testee {
  //testee_SDAG_CODE

  CProxy_testee m_parent;
  bool          m_is_root;
  int           m_num_received_results;
  int           m_results[2];

 public:
  testee(bool is_root, CProxy_testee parent)
      : m_parent(parent), m_is_root(is_root), m_num_received_results(0) {
    // calc(s_num);
  }

  void spread(int num) {
    if (num == 1) {
      m_parent.result(1);
      delete this;
    } else {
      CProxy_testee child1 = CProxy_testee::ckNew(false, thisProxy);
      CProxy_testee child2 = CProxy_testee::ckNew(false, thisProxy);
      child1.spread(num - 1);
      child2.spread(num - 1);
    }
  }

  void result(int value) {
    m_results[m_num_received_results++] = value;
    if (m_num_received_results == 2) {
      if (m_is_root) {
        //std::cout << "result: " << (2 + m_results[0] + m_results[1])
        //          << std::endl;
        CkExit();
      } else {
        m_parent.result(1 + m_results[0] + m_results[1]);
        delete this;
      }
    }
  }
};

#include "charm_actor_creation.def.h"

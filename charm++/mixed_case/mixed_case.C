#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <functional>

#include "charm++.h"
#include "mixed_case.decl.h"


typedef std::vector<uint64_t> factors;

/*readonly*/ bool     debug      = false;
/*readonly*/ uint64_t s_factor1  = 86028157;
/*readonly*/ uint64_t s_factor2  = 329545133;
/*readonly*/ uint64_t s_task_n   = s_factor1 * s_factor2;

inline void check_factors(const factors& vec) {
  using namespace std;
  assert(vec.size() == 2);
  assert(vec[0] == s_factor1);
  assert(vec[1] == s_factor2);
# ifdef NDEBUG
  static_cast<void>(vec);
# endif
}

class chain_master : public CBase_chain_master {
  chain_master_SDAG_CODE
 public:
  chain_master(CProxy_supervisor sv) : m_sv(sv), m_itr(0) {
    // nop
    m_worker = CProxy_worker::ckNew(m_sv);
  }

  void token(int value) {
    if (value == 0) {
      if (++m_itr < m_n) {
        if(debug) std::cout << "new ring: " << m_itr << std::endl;
        new_ring();
      } else {
        m_worker.done();
        m_sv.masterdone();
        delete this;
      }
    } else {
      m_next.token(value - 1);
    }
  }

 private:
  CProxy_chain_link m_next;
  CProxy_worker     m_worker;
  CProxy_supervisor m_sv;
  int               m_itr;
  int               m_itv;
  int               m_rs;
  int               m_n;
};

class supervisor : public CBase_supervisor {
  supervisor_SDAG_CODE
 public:

  supervisor(int num_msgs) : m_left(num_msgs) {
    // nop
  }

 private:
  int m_left;
};

class worker : public CBase_worker {
 public:
  worker(CProxy_supervisor sv) : m_sv(sv) {
    if(debug) std::cout << "New worker created" << std::endl;
    // nop
  }

  void calc(uint64_t what) {
    std::vector<uint64_t> result;
    factorize(result, what);
    if(debug) std::cout << "worker: before result" << std::endl;
    m_sv.result(result);
    if(debug) std::cout << "worker: after result" << std::endl;
  }
  
  void done() {
    // quit charm?
    delete this; 
  } 

 private:
  void factorize(std::vector<uint64_t>& result, uint64_t n) {
    if (n <= 3) {
      result.push_back(n);
    }
    uint64_t d = 2;
    while (d < n) {
      if ((n % d) == 0) {
        result.push_back(d);
        n /= d;
      } else {
        d = (d == 2) ? 3 : (d + 2);
      }
    }
    result.push_back(d);
  }

  CProxy_supervisor m_sv;
};

int link_no = 0;

class chain_link : public CBase_chain_link {
 public:

  chain_link(CProxy_chain_link next, std::string msg) : m_next(next) {
    link_no++;
    if(debug) std::cout << "next isnt master " << msg << std::endl;
    m_next_is_master = false;
  }

  chain_link(CProxy_chain_master next, std::string msg) : m_next_master(next) {
    link_no++;
    if(debug) std::cout << "next is master" << msg << std::endl;
    m_next_is_master = true;
  }

  void token(int value) {
    if (debug) std::cout << "chainlink received token " << value << std::endl;
    if(value != 0) --value;
    if (m_next_is_master) {
      m_next_master.token(value);
    } else {
      m_next.token(value);
    }
    if (value == 0) {
      // quit charm?
      delete this;
    }
  }

 private:

  bool                m_next_is_master;
  CProxy_chain_link   m_next;
  CProxy_chain_master m_next_master;
};

struct main : public CBase_main {
  void usage() {
    using namespace std;
    cout << endl << endl
         << "usage: ./mixed_case "
         << "NUM_RINGS RING_SIZE INITIAL_TOKEN_VALUE REPETITIONS"
         << endl
         << endl;
    CkExit();
  }   
   
  main(CkArgMsg* m) {
    using namespace std;
    if (m->argc != 5) {
      usage();
    }
    int num_rings;
    int ring_size;
    int initial_token_value;
    int repetitions;
    try {
      num_rings           = atoi(m->argv[1]);
      ring_size           = atoi(m->argv[2]);
      initial_token_value = atoi(m->argv[3]);
      repetitions         = atoi(m->argv[4]);
    } catch(std::exception&) {
      usage();
    }
    delete m;
    int num_msgs = num_rings + (num_rings * repetitions);
    CProxy_supervisor sv = CProxy_supervisor::ckNew(num_msgs);
    std::vector<CProxy_chain_master> masters;
    for (int i = 0; i < num_rings; ++i) {
      if(debug) std::cout << "rings: " << i << std::endl;
      CProxy_chain_master master 
          = CProxy_chain_master::ckNew(sv);
      master.init(ring_size, initial_token_value, repetitions);
      masters.push_back(master);
    }
    if (debug) std::cout << "All masters created" << std::endl;
  }
};

#include "mixed_case.def.h"

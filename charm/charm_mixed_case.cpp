#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <functional>

#include "charm++.h"
#include "mixed_case.decl.h"


typedef std::vector<uint64_t> factors;

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

void factorize(std::vector<uint64_t>& result, uint64_t n) {
  if (n <= 3) {
    result.push_back(n);
    return;
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
      CProxy_chain_master master =
          CProxy_chain_master::ckNew(sv, ring_size,
                                     initial_token_value, repetitions);
      master.init();
      masters.push_back(master);
    }
  }
};


class chain_master : public CBase_chain_master {
 public:
  chain_master(CProxy_supervisor msgcollector, int rs, int itv, int n)
      : iteration(0),
        mc(msgcollector),
        ring_size(rs),
        initial_token_value(itv),
        max_iterations(n) {
    // nop
  }

  void init() {
    factorizer = CProxy_worker::ckNew(mc);
    iteration = 0;
    new_ring();
  }

  void token(int value) {
    if (value == 0) {
      if (++iteration < max_iterations) {
        new_ring();
      } else {
        factorizer.done();
        mc.masterdone();
      }
    } else {
      next.token(value - 1);
    }
  }

 private:
  void new_ring() {
    factorizer.calc(s_task_n);
    CProxy_chain_link next = CProxy_chain_link::ckNew(thisProxy, 42);
    for (int i = 2; i < ring_size; ++i) {
      next = CProxy_chain_link::ckNew(next);
    }
    next.token(initial_token_value);
  }

  int               iteration;
  CProxy_chain_link next;
  CProxy_worker     factorizer;
  CProxy_supervisor mc;
  int ring_size;
  int initial_token_value;
  int max_iterations;
};

class supervisor : public CBase_supervisor {
 public:
  supervisor(int num_msgs) : m_left(num_msgs) {
    // nop
  }

  void masterdone() {
    if (--m_left == 0) {
      CkExit();
    }
  }

  void result(const factors&) {
    if (--m_left == 0) {
      CkExit();
    }
  }

 private:
  int m_left;
};

class worker : public CBase_worker {
 public:
  worker(CProxy_supervisor sv) : m_sv(sv) {
    // nop
  }

  void calc(uint64_t what) {
    std::vector<uint64_t> result;
    factorize(result, what);
    m_sv.result(result);
  }

  void done() {
    delete this;
  }

 private:
  CProxy_supervisor m_sv;
};

class chain_link : public CBase_chain_link {
 public:
  chain_link(CProxy_chain_link next) : m_next(next), m_next_is_master(false) {
    // nop
  }

  chain_link(CProxy_chain_master next, int msg)
      : m_next_master(next), m_next_is_master(true) {
    // nop
  }

  void token(int value) {
    if (m_next_is_master) {
      m_next_master.token(value);
    } else {
      m_next.token(value);
    }
    if (value == 0) {
      delete this;
    }
  }

 private:

  bool                m_next_is_master;
  CProxy_chain_link   m_next;
  CProxy_chain_master m_next_master;
};

#include "mixed_case.def.h"

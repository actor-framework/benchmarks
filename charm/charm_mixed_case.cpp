#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <functional>

#include "charm++.h"
#include "charm_mixed_case.decl.h"


/*readonly*/ uint64_t s_factor1  = 86028157;
/*readonly*/ uint64_t s_factor2  = 329545133;
/*readonly*/ uint64_t s_task_n   = s_factor1 * s_factor2;

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
    int num_rings = atoi(m->argv[1]);
    int ring_size = atoi(m->argv[2]);
    int initial_token_value = atoi(m->argv[3]);
    int repetitions = atoi(m->argv[4]);
    delete m;
    int num_msgs = num_rings + (num_rings * repetitions);
    CProxy_supervisor sv = CProxy_supervisor::ckNew(num_msgs);
    for (int i = 0; i < num_rings; ++i) {
      CProxy_chain_master master =
          CProxy_chain_master::ckNew(sv, ring_size,
                                     initial_token_value, repetitions);
      master.init();
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
    //std::cout << "master::init" << std::endl;
    factorizer = CProxy_worker::ckNew(mc);
    new_ring();
  }

  void token(int value) {
    //std::cout << "master::token " << value << std::endl;
    if (value == 0) {
      if (++iteration < max_iterations) {
        new_ring();
      } else {
        factorizer.done();
        mc.masterdone();
        delete this;
      }
    } else {
      next.token(value - 1);
    }
  }

 private:
  void new_ring() {
    // without the dummy elements, Charm++ tries to create an infinite
    // number of link nodes for whatever reason
    //std::cout << "master::new_ring(" << ring_size << ")" << std::endl;
    factorizer.calc(s_task_n);
    // spawning the ring on a single PE boosts
    // performance quite significantly
    next = CProxy_chain_link::ckNew(thisProxy, 42, CkMyPe());
    for (int i = 2; i < ring_size; ++i) {
      next = CProxy_chain_link::ckNew(next, 0.0, CkMyPe());
    }
    //std::cout << "master -> next ! " << initial_token_value << std::endl;
    next.token(initial_token_value);
  }

  int iteration;
  CProxy_supervisor mc;
  int ring_size;
  int initial_token_value;
  int max_iterations;
  CProxy_chain_link next;
  CProxy_worker factorizer;
};

class supervisor : public CBase_supervisor {
 public:
  supervisor(int num_msgs) : left(num_msgs) {
    // nop
  }

  void masterdone() {
    if (--left == 0) {
      CkExit();
    }
  }

  void result(const std::vector<uint64_t>&) {
    if (--left == 0) {
      CkExit();
    }
  }

 private:
  int left;
};

class worker : public CBase_worker {
 public:
  worker(CProxy_supervisor sv) : msgcollector(sv) {
    // nop
  }

  void calc(uint64_t what) {
    std::vector<uint64_t> result;
    factorize(result, what);
    msgcollector.result(result);
  }

  void done() {
    delete this;
  }

 private:
  CProxy_supervisor msgcollector;
};

class chain_link : public CBase_chain_link {
 public:
  chain_link(CProxy_chain_link next, double) : next_is_master(false), next(next) {
    // nop
    //std::cout << "link::link(link)" << std::endl;
  }

  chain_link(CProxy_chain_master next, int) : next_is_master(true), mnext(next) {
    // nop
    //std::cout << "link::link(master)" << std::endl;
  }

  void token(int value) {
    //std::cout << "link::token " << value << " [" << std::boolalpha << next_is_master << "]" << std::endl;
    if (next_is_master) {
      mnext.token(value);
    } else {
      next.token(value);
    }
    if (value == 0) {
      delete this;
    }
  }

 private:
  bool                next_is_master;
  CProxy_chain_link   next;
  CProxy_chain_master mnext;
};

#include "charm_mixed_case.def.h"

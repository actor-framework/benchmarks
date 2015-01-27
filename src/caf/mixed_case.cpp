/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>

#include "caf/all.hpp"

using std::cout;
using std::endl;

using namespace caf;

namespace {

using factors = std::vector<uint64_t>;

constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;
constexpr uint64_t s_task_n = s_factor1 * s_factor2;

using calc_atom = atom_constant<atom("calc")>;
using done_atom = atom_constant<atom("done")>;
using token_atom = atom_constant<atom("token")>;

} // namespace <anonymous>

factors factorize(uint64_t n) {
  factors result;
  if (n <= 3) {
    result.push_back(n);
    return result;
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
  return result;
}

inline void check_factors(const factors& vec) {
    assert(vec.size() == 2);
    assert(vec[0] == s_factor1);
    assert(vec[1] == s_factor2);
#   ifdef NDEBUG
    static_cast<void>(vec);
#   endif
}

behavior worker(event_based_actor* self) {
  return {
    [](calc_atom, uint64_t what) {
      return factorize(what);
    },
    [=](done_atom) {
      self->quit();
    }
  };
}

behavior chain_link(event_based_actor* self, const actor& next) {
  return {
    [=](token_atom, uint64_t value) {
      if (value == 0) self->quit();
      self->forward_to(next);
    }
  };
}

class chain_master : public event_based_actor {
 public:
    chain_master(actor msgcollector, int rs, uint64_t itv, int n)
      : m_iteration(0),
        m_ring_size(rs),
        m_initial_token_value(itv),
        m_num_iterations(n),
        m_mc(msgcollector) {
      m_factorizer = spawn<detached>(worker);
      new_ring();
    }
    ~chain_master();
    behavior make_behavior() override {
      return {
        [=](token_atom, uint64_t& value) {
          if (value == 0) {
            if (++m_iteration < m_num_iterations) {
              new_ring();
            } else {
              send(m_factorizer, done_atom::value);
              send(m_mc, done_atom::value);
              quit();
            }
          } else {
            value -= 1;
            forward_to(m_next);
          }
        }
      };
    }

 private:
  void new_ring() {
    send_as(m_mc, m_factorizer, calc_atom::value, s_task_n);
    m_next = this;
    for (int i = 1; i < m_ring_size; ++i) {
      m_next = spawn(chain_link, m_next);
    }
    send(m_next, token_atom::value, m_initial_token_value);
  }
  int m_iteration;
  int m_ring_size;
  uint64_t m_initial_token_value;
  int m_num_iterations;
  actor m_mc;
  actor m_next;
  actor m_factorizer;
};

chain_master::~chain_master() {
  // nop
}

class supervisor : public event_based_actor{
 public:
  supervisor(int num_msgs) : m_left(num_msgs) {
    // nop
  }
  ~supervisor();
  behavior make_behavior() override {
    return {
      [=](done_atom) {
        if (--m_left == 0) {
          quit();
        }
      },
      [=](const factors& vec) {
        check_factors(vec);
        if (--m_left == 0) {
          quit();
        }
      }
    };
  }
 private:
  int m_left;
};

supervisor::~supervisor() {
  // nop
}

int main(int argc, char** argv) {
  if (argc != 5) {
    cout << "usage: mixed_case "
            "NUM_RINGS RING_SIZE INITIAL_TOKEN_VALUE REPETITIONS"
         << endl << endl;
    return 1;
  }
  announce<factors>("factors");
  int num_rings = atoi(argv[1]);
  int ring_size = atoi(argv[2]);
  auto initial_token_value = static_cast<uint64_t>(atoi(argv[3]));
  int repetitions = atoi(argv[4]);
  auto sv = spawn<supervisor>(num_rings + (num_rings * repetitions));
  for (int i = 0; i < num_rings; ++i) {
    spawn<chain_master>(sv, ring_size, initial_token_value, repetitions);
  }
  await_all_actors_done();
}

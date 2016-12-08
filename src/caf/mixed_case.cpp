/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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
    [=](token_atom tk, uint64_t value) {
      if (value == 0)
        self->quit();
      self->delegate(next, tk, value);
    }
  };
}

class chain_master : public event_based_actor {
 public:
    chain_master(actor_config& cfg, actor coll, int rs, uint64_t itv, int n)
      : event_based_actor(cfg),
        iteration_(0),
        ring_size_(rs),
        m_initial_token_value(itv),
        num_iterations_(n),
        mc_(coll),
        next_(this),
        factorizer_(spawn<detached>(worker)) {
      // nop
    }

    behavior make_behavior() override {
      new_ring();
      return {
        [=](token_atom tk, uint64_t value) {
          if (value == 0) {
            if (++iteration_ < num_iterations_) {
              new_ring();
            } else {
              send(factorizer_, done_atom::value);
              send(mc_, done_atom::value);
              quit();
            }
          } else {
            value -= 1;
            delegate(next_, tk, value);
          }
        }
      };
    }

 private:
  void new_ring() {
    send_as(mc_, factorizer_, calc_atom::value, s_task_n);
    next_ = this;
    for (int i = 1; i < ring_size_; ++i)
      next_ = spawn<lazy_init>(chain_link, next_);
    send(next_, token_atom::value, m_initial_token_value);
  }
  int iteration_;
  int ring_size_;
  uint64_t m_initial_token_value;
  int num_iterations_;
  actor mc_;
  actor next_;
  actor factorizer_;
};

class supervisor : public event_based_actor {
 public:
  supervisor(actor_config& cfg, int num_msgs)
      : event_based_actor(cfg),
        left_(num_msgs) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](const factors& vec) {
        check_factors(vec);
        if (--left_ == 0)
          quit();
      },
      [=](done_atom) {
        if (--left_ == 0)
          quit();
      }
    };
  }

 private:
  int left_;
};

} // namespace <anonymous>

int main(int argc, char** argv) {
  if (argc != 5)
    return cout << "usage: mixed_case "
                   "NUM_RINGS RING_SIZE INITIAL_TOKEN_VALUE REPETITIONS"
                << endl << endl, 1;
  auto num_rings = atoi(argv[1]);
  auto ring_size = atoi(argv[2]);
  auto initial_token_value = static_cast<uint64_t>(atoi(argv[3]));
  auto repetitions = atoi(argv[4]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  cfg.add_message_type<factors>("factors");
  actor_system system{cfg};
  auto sv = system.spawn<supervisor, lazy_init>(num_rings
                                                + (num_rings * repetitions));
  for (int i = 0; i < num_rings; ++i)
    system.spawn<chain_master>(sv, ring_size, initial_token_value, repetitions);
}



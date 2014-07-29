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

using namespace std;
using namespace caf;

typedef vector<uint64_t> factors;

constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;
constexpr uint64_t s_task_n = s_factor1 * s_factor2;

vector<uint64_t> factorize(uint64_t n) {
  vector<uint64_t> result;
  if (n <= 3) {
    result.push_back(n);
    return move(result);
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
  return move(result);
}

inline void check_factors(const factors& vec) {
    assert(vec.size() == 2);
    assert(vec[0] == s_factor1);
    assert(vec[1] == s_factor2);
#   ifdef NDEBUG
    static_cast<void>(vec);
#   endif
}

class worker : public event_based_actor {
 public:
  worker(const actor& msgcollector) : mc(msgcollector) {
    // nop
  }
  behavior make_behavior() override {
    return {
      on<atom("calc"), uint64_t>() >> [=](uint64_t what) {
        send(mc, atom("result"), factorize(what));
      },
      on(atom("done")) >> [=]() {
        quit();
      }
    };
  }
 private:
  actor mc;
  behavior init_state;
};

class chain_link : public event_based_actor {
 public:
  chain_link(const actor& n) : next(n) {
    // nop
  }
  behavior make_behavior() override {
    return {
      on<atom("token"), int>() >> [=](int v) {
        send_tuple(next, std::move(last_dequeued()));
        if (v == 0) quit();
      }
    };
  }
 private:
  actor next;
  behavior init_state;
};

class chain_master : public event_based_actor {
 public:
    chain_master(actor msgcollector) : iteration(0), mc(msgcollector) {
      // nop
    }
    behavior make_behavior() override {
      return {
        on(atom("init"), arg_match) >> [=](int rs, int itv, int n) {
          factorizer = spawn<worker>(mc);
          iteration = 0;
          new_ring(rs, itv);
          become (
            on(atom("token"), 0) >> [=]() {
              if (++iteration < n) {
                new_ring(rs, itv);
              } else {
                send(factorizer, atom("done"));
                send(mc, atom("masterdone"));
                quit();
              }
            },
            on<atom("token"), int>() >> [=](int v) {
              send(next, atom("token"), v - 1);
            }
          );
        }
      };
    }
 private:
  void new_ring(int ring_size, int initial_token_value) {
    send(factorizer, atom("calc"), s_task_n);
    next = this;
    for (int i = 1; i < ring_size; ++i) {
      next = spawn<chain_link>(next);
    }
    send(next, atom("token"), initial_token_value);
  }
  int iteration;
  actor mc;
  actor next;
  actor factorizer;
  behavior init_state;
};

class supervisor : public event_based_actor{
 public:
  supervisor(int num_msgs) : left(num_msgs) {
    // nop
  }
  behavior make_behavior() override {
    return {
      on(atom("masterdone")) >> [=]() {
        if (--left == 0) quit();
      },
      on<atom("result"), factors>() >> [=](const factors& vec) {
        check_factors(vec);
        if (--left == 0) quit();
      }
    };
  }
 private:
  int left;
  behavior init_state;
};

void usage() {
    cout << "usage: mailbox_performance "
            "[--stacked] (num rings) (ring size) "
            "(initial token value) (repetitions)"
         << endl
         << endl;
    exit(1);
}

void run(int num_rings, int ring_size,
         int initial_token_value, int repetitions) {
  int num_msgs = num_rings + (num_rings * repetitions);
  auto sv = spawn<supervisor>(num_msgs);
  std::vector<actor> masters; // of the universe
  // each master sends one masterdone message and one
  // factorization is calculated per repetition
  // auto supermaster = spawn(supervisor, num_rings+repetitions);
  for (int i = 0; i < num_rings; ++i) {
    masters.push_back(spawn<chain_master>(sv));
    anon_send(masters.back(), atom("init"), ring_size,
              initial_token_value, repetitions);
  }
}

int main(int argc, char** argv) {
  if (argc != 5) {
    usage();
  }
  try {
    run(stoi(argv[1]), stoi(argv[2]), stoi(argv[3]), stoi(argv[4]));
  }
  catch (std::exception&) {
    usage();
  }
  await_all_actors_done();
}

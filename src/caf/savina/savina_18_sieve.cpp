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

#include <iostream>
#include <vector>
#include <algorithm>
#include <map>

#include "caf/all.hpp"

using namespace std;
using std::chrono::seconds;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 100000;
  int m = 1000;
  static bool debug; // = false;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "search limit for primes") 
      .add(m, "mmm,m", "maximum primes storage capacity per actor")
      .add(debug, "ddd,d", "debug");
  }
};
bool config::debug = false;

struct long_box {
  long value;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(long_box);

bool is_locally_prime(long candidate, const vector<long>& local_primes,
                      int start_inc, int end_exc) {
  for (int i = start_inc; i < end_exc; ++i) {
    long remainder = candidate % local_primes[i];
    if (remainder == 0) {
      return false; 
    }
  }
  return true;
}

behavior number_producer_actor(event_based_actor* self, long limit) {
  return {
    [=](actor filter_actor) {
      long candidate = 3;
      while (candidate < limit) {
        self->send(filter_actor, long_box{candidate});
        candidate += 2;
      }
      self->send(filter_actor, string("EXIT"));
      self->quit();
    } 
  };
}

struct prime_filter_actor_state {
  actor next_filter_actor;
  vector<long> local_primes;
  int available_local_primes; 
};

behavior prime_filter_actor_fun(stateful_actor<prime_filter_actor_state>* self,
                                int id, long my_initial_prime,
                                int num_max_local_primes) {
  auto& s = self->state;
  s.local_primes.reserve(num_max_local_primes);
  for (int i = 0; i < num_max_local_primes; ++i) {
    s.local_primes.emplace_back(0); 
  }
  s.available_local_primes = 1;
  s.local_primes[0] = my_initial_prime;
  auto handle_new_prime = [=](long new_prime) mutable {
    auto& s = self->state;
    if (config::debug) {
      aout(self) << "Found new prime number " << new_prime << endl;
    } 
    if (s.available_local_primes < num_max_local_primes) {
      // Store locally if there is space
      s.local_primes[s.available_local_primes] = new_prime;
      ++s.available_local_primes;
    } else {
      // Create a new actor to store the new prime
      s.next_filter_actor = self->spawn(prime_filter_actor_fun, id + 1,
                                        new_prime, num_max_local_primes);
    }
  };
  return {
    [=](long_box& candidate) mutable {
      auto& s = self->state;
      auto locally_prime = is_locally_prime(candidate.value, s.local_primes, 0,
                                            s.available_local_primes);
      if (locally_prime) {
        if (s.next_filter_actor) {
          // Pass along the chain to detect for 'primeness'
          self->send(s.next_filter_actor, move(candidate)); 
        } else {
          // Found a new prime! 
          handle_new_prime(candidate.value);
        }
      }
    },
    [=](string& x) {
      auto& s = self->state;
      if (!s.next_filter_actor)  {
        // Signal next actor to terminate
        self->send(s.next_filter_actor, move(x));
      } else {
        auto total_primes =
          ((id - 1) * num_max_local_primes) + s.available_local_primes;
        aout(self) << "Total primes = " << total_primes << endl;
      }
      if (config::debug) {
        aout(self) << "Terminating prime actor for number " << my_initial_prime
                   << endl;
        ;
      }
      self->quit();
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  auto producer_actor = system.spawn(number_producer_actor, cfg.n);
  auto filter_actor = system.spawn(prime_filter_actor_fun, 1, 2, cfg.m);
  anon_send(producer_actor, filter_actor);
}

CAF_MAIN()

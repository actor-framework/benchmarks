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
#include <cstdlib>

#include "caf/all.hpp"

#include "benchmark_helper.hpp"

using namespace std;
using std::chrono::seconds;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 100000;
  long long m = 1l << 60;
  long long s = 2048;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "data size")
      .add(m, "mmm,m", "maximum value")
      .add(s, "sss,s", "seed for random number generator");
  }

};

struct next_actor_msg {
  actor next_actor;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(next_actor_msg);

struct value_msg {
  long long value;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(value_msg);

behavior int_source_actor_fun(event_based_actor* self, int num_values,
                              long long max_value, long long seed) {
  pseudo_random random(seed);
  return {
    [=](next_actor_msg& nm) mutable {
      for (int i = 0; i < num_values; ++i) {
        auto candidate = abs(random.next_long()) % max_value;
        auto message = value_msg{candidate};
        self->send(nm.next_actor, move(message));
      }
      self->quit();
    } 
  };
}

struct sort_actor_state {
  vector<value_msg> ordering_array;
};

behavior sort_actor_fun(stateful_actor<sort_actor_state>* self, int num_values,
                        long long radix, actor next_actor) {
  auto& s = self->state;
  s.ordering_array.reserve(num_values);
  for (int i = 0; i < num_values; ++i) {
    s.ordering_array.emplace_back(value_msg());
  }
  int values_so_far = 0;  
  int j = 0; 
  return  {
    [=](value_msg& vm) mutable {
      auto& s = self->state;
      ++values_so_far;
      auto current = vm.value;
      if ((current & radix) == 0) {
        self->send(next_actor, move(vm)); 
      } else {
        s.ordering_array[j] = move(vm);
        ++j;
      }
      if (values_so_far == num_values) {
        for (int i = 0; i < j; ++i) {
          self->send(next_actor, s.ordering_array[i]);
        }
        self->quit();
      }
    } 
  };
}

behavior validation_actor_fun(event_based_actor* self, int num_values) {
  auto sum_so_far = 0.0;
  auto values_so_far = 0;
  auto prev_value = 0L;
  auto error_value = make_tuple(-1L, -1); 
  return {
    [=](value_msg& vm) mutable {
      ++values_so_far; 
      if (vm.value < prev_value && get<0>(error_value) < 0) {
        error_value = make_tuple(vm.value, values_so_far -1);
      }
      prev_value = vm.value;
      sum_so_far += prev_value;
      if (values_so_far == num_values) {
        if (get<0>(error_value) >= 0) {
          cout << "ERROR: Value out of place: " << get<0>(error_value)
               << " at index " << get<1>(error_value) << endl;
        } else {
          cout << "Elements sum: " << sum_so_far << endl;
        }
        self->quit();
      }
    } 
  };
}

void caf_main(actor_system& system, const config& cfg) {
  auto validation_actor = system.spawn(validation_actor_fun, cfg.n);
  auto source_actor = system.spawn(int_source_actor_fun, cfg.n, cfg.m, cfg.s);
  auto radix = cfg.m / 2;
  auto next_actor = validation_actor;
  while (radix > 0) {
    auto local_radix = radix; 
    auto local_next_actor = next_actor;
    auto sort_actor = system.spawn(sort_actor_fun, cfg.n, local_radix, local_next_actor);
    radix /= 2;
    next_actor = sort_actor;
  }
  anon_send(source_actor, next_actor_msg{next_actor});
}

CAF_MAIN()

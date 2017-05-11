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
#include <atomic>

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using start_atom = atom_constant<atom("start")>;
using exit_atom = atom_constant<atom("exit")>;
using hungry_atom = atom_constant<atom("hungry")>;
using done_atom = atom_constant<atom("done")>;
using eat_atom = atom_constant<atom("eat")>;
using denied_atom = atom_constant<atom("denied")>;

class config : public actor_system_config {
public:
  int n = 20;
  int m = 10000;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of philosophers")
      .add(m, "mmm,m", "number of eating rounds");
  }
};

struct philosopher_states {
  long local_counter = 0;
  int rounds_so_far = 0;
};

behavior philosopher_actor(stateful_actor<philosopher_states>* self, int id,
                           int rounds, atomic_long* counter, actor arbitrator) {
  return {
    [=](denied_atom) {
      auto& s = self->state;
      ++s.local_counter;
      self->send(arbitrator, hungry_atom::value, id);
    },
    [=](eat_atom) {
      auto& s = self->state;
      ++s.rounds_so_far;
      counter->fetch_add(s.local_counter);
      self->send(arbitrator, done_atom::value, id);
      if (s.rounds_so_far < rounds) {
        self->send(self, start_atom::value);
      } else {
        self->send(arbitrator, exit_atom::value);
        self->quit();
      }
    },
    [=](start_atom) { 
      self->send(arbitrator, hungry_atom::value, id); 
    }
  };
}

struct arbitrator_actor_state {
  vector<bool> forks;
  int num_exit_philosophers = 0;
};

behavior arbitrator_actor(stateful_actor<arbitrator_actor_state>* self,
                          int num_forks) {
  auto& s = self->state;
  s.forks.reserve(num_forks);
  for (int i = 0; i < num_forks; ++i) {
    s.forks.emplace_back(false); // fork not used
  }
  return {
    [=](hungry_atom, int philosopher_id) {
      auto& s = self->state;
      auto left_fork = philosopher_id;
      auto right_fork = (philosopher_id + 1) % s.forks.size();
      if (s.forks[left_fork] || s.forks[right_fork]) {
        self->send(actor_cast<actor>(self->current_sender()),
                   denied_atom::value);
      } else {
        s.forks[left_fork] = true;
        s.forks[right_fork] = true;
        self->send(actor_cast<actor>(self->current_sender()),
                   eat_atom::value);
      }
    },
    [=](done_atom, int philosopher_id) {
      auto& s = self->state;
      auto left_fork = philosopher_id;
      auto right_fork = (philosopher_id + 1) % s.forks.size();
      s.forks[left_fork] = false;
      s.forks[right_fork] = false;
    },
    [=](exit_atom) {
      auto& s = self->state;
      ++s.num_exit_philosophers;
      if (num_forks == s.num_exit_philosophers) {
        self->quit();
      }
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  atomic_long counter{0};
  auto arbitrator = system.spawn(arbitrator_actor, cfg.n);
  vector<actor> philosophers;
  philosophers.reserve(cfg.n);  
  for (int i = 0; i < cfg.n; ++i) {
    philosophers.emplace_back(system.spawn(philosopher_actor, i, cfg.m, &counter, arbitrator));
  }
  for (auto& loop_actor : philosophers) {
    anon_send(loop_actor, start_atom::value);
  }
  system.await_all_actors_done();
  cout << "Num retries: "<< counter << endl;
}

CAF_MAIN()

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
#include <deque>

#include "caf/all.hpp"

using namespace std;
using std::chrono::seconds;
using namespace caf;

class config : public actor_system_config {
public:
  static int num_terms; // = 25000;
  static int num_series; // = 10;
  static double start_rate; // = 3.46;
  static double increment; // = 0.0025;

  config() {
    opt_group{custom_options_, "global"}
      .add(num_terms, "ttt,t", "number of terms")
      .add(num_series, "sss,s", "number of series")
      .add(start_rate, "rrr,r", "start rate");
  }
};
int config::num_terms = 25000;
int config::num_series = 10;
double config::start_rate = 3.46;
double config::increment = 0.0025;

double compute_next_term(double cur_term, double rate) {
  return rate * cur_term * (1 - cur_term);
}

using start_msg_atom = atom_constant<atom("startmsg")>;
using stop_msg_atom = atom_constant<atom("stopmsg")>;
using next_term_msg_atom = atom_constant<atom("nextmsg")>;
using get_term_msg_atom = atom_constant<atom("getmsg")>;

struct compute_msg {
  actor sender;
  double term;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(compute_msg);

struct result_msg {
  double term;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(result_msg);

#ifdef REQUEST_AWAIT
struct series_worker_state {
  vector<double> cur_term;
};

behavior series_worker_fun(stateful_actor<series_worker_state>* self, int /*id*/,
                           actor master, actor computer, double start_term) {
  auto& s = self->state;
  s.cur_term.emplace_back(start_term);
  return {
    [=](next_term_msg_atom) {
      auto& s = self->state;
      auto sender = actor_cast<actor>(self);
      auto new_message = compute_msg{move(sender), s.cur_term[0]};
  #ifdef INFINITE
      self->request(computer, infinite, move(new_message)).await(
  #elif HIGH_TIMEOUT
      self->request(computer, seconds(6000), move(new_message)).await(
  #endif
        [=](result_msg& result_message) {
          self->state.cur_term[0] = result_message.term;
        }); 
    },
    [=](get_term_msg_atom) {
      self->send(master, result_msg{self->state.cur_term[0]});
    },
    [=](stop_msg_atom) {
      self->quit(); 
    }
  };
}
#elif BECOME_UNBECOME_FAST
struct series_worker_state {
  vector<double> cur_term;
  behavior wait_for_result;
};

behavior series_worker_fun(stateful_actor<series_worker_state>* self, int /*id*/,
                           actor master, actor computer, double start_term) {
  auto& s = self->state;
  self->set_default_handler(skip);
  s.cur_term.emplace_back(start_term);
  s.wait_for_result = {
    [=](result_msg& result_message) {
      self->state.cur_term[0] = result_message.term;
      self->unbecome();
    }
  };
  return {
    [=](next_term_msg_atom) {
      auto& s = self->state;
      auto sender = actor_cast<actor>(self);
      auto new_message = compute_msg{move(sender), s.cur_term[0]};
      self->send(computer, move(new_message)); 
      self->become(keep_behavior, s.wait_for_result);
    },
    [=](get_term_msg_atom) {
      self->send(master, result_msg{self->state.cur_term[0]});
    },
    [=](stop_msg_atom) {
      self->quit(); 
    }
  };
}
#elif BECOME_UNBECOME_SLOW
struct series_worker_state {
  vector<double> cur_term;
};

behavior series_worker_fun(stateful_actor<series_worker_state>* self, int /*id*/,
                           actor master, actor computer, double start_term) {
  auto& s = self->state;
  self->set_default_handler(skip);
  s.cur_term.emplace_back(start_term);
  return {
    [=](next_term_msg_atom) {
      auto& s = self->state;
      auto sender = actor_cast<actor>(self);
      auto new_message = compute_msg{move(sender), s.cur_term[0]};
      self->send(computer, move(new_message)); 
      self->become(keep_behavior, 
        [=](result_msg& result_message) {
          self->state.cur_term[0] = result_message.term;
          self->unbecome();
        });
    },
    [=](get_term_msg_atom) {
      self->send(master, result_msg{self->state.cur_term[0]});
    },
    [=](stop_msg_atom) {
      self->quit(); 
    }
  };
}
#endif

behavior rate_computer_fun(event_based_actor* self, double rate) {
  return {
    [=](compute_msg& compute_message) {
      auto result = compute_next_term(compute_message.term, rate); 
      return result_msg{result};
    },
    [=](stop_msg_atom) {
      self->quit(); 
    }
  };
}

struct master_state {
  vector<actor> computers;
  vector<actor> workers;
  int num_work_requested = 0;
  int num_work_received = 0;
  double terms_sum = 0;
};

behavior master_fun(stateful_actor<master_state>* self) {
  auto& s = self->state;
  int num_computers = config::num_series;
  s.computers.reserve(num_computers);
  for (int i = 0; i < num_computers; ++i) {
    auto rate = config::start_rate + (i * config::increment); 
    s.computers.emplace_back(self->spawn(rate_computer_fun, rate));
  }
  int num_workers = config::num_series;
  s.workers.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    auto& rate_computer = s.computers[i % num_computers];
    auto start_term = i * config::increment;
    s.workers.emplace_back(self->spawn(series_worker_fun, i,
                                       actor_cast<actor>(self), rate_computer,
                                       start_term));
  }
  return {
    [=](start_msg_atom) {
      auto& s = self->state;
      for (int i = 0; i < config::num_terms; ++i) {
        for (auto& loop_worker : s.workers) {
          self->send(loop_worker, next_term_msg_atom::value); 
        }  
      } 
      for (auto& loop_worker : s.workers) {
        self->send(loop_worker, get_term_msg_atom::value);
        ++s.num_work_requested;
      }
    },
    [=](result_msg& rm) {
      auto& s = self->state;
      s.terms_sum += rm.term;
      ++s.num_work_received;
      if (s.num_work_requested == s.num_work_received) {
        cout << "Terms sum: " << s.terms_sum << endl;
        for (auto& loop_computer: s.computers) {
          self->send(loop_computer, stop_msg_atom::value);
        }
        for (auto& loop_worker : s.workers) {
          self->send(loop_worker, stop_msg_atom::value);
        }
        self->quit();
      }
    }
  };
}

void caf_main(actor_system& system, const config& /*cfg*/) {
  auto master = system.spawn(master_fun);
  anon_send(master, start_msg_atom::value);
}

CAF_MAIN()

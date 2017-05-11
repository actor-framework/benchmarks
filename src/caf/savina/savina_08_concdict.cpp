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
#include <unordered_map>

#include "caf/all.hpp"

#include "benchmark_helper.hpp"

using namespace std;
using namespace caf;

struct write_msg {
  actor sender;
  int key;
  int value;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(write_msg);

struct read_msg {
  actor sender;
  int key;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(read_msg);

struct result_msg {
  actor sender;
  int key;
};
const auto do_work_msg = result_msg{actor(),-1};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(result_msg);

using end_work_msg_atom = atom_constant<atom("endwork")>;

class config : public actor_system_config {
public:
  int num_entitites = 20;
  int num_msgs_per_worker = 10000;
  static int write_percentage; // = 10;
  using data_map = unordered_map<int, int>;
  
  config() {
    opt_group{custom_options_, "global"}
    .add(num_entitites, "eee,e", "number of entities")
    .add(num_msgs_per_worker, "mmm,m", "number of messges per worker")
    .add(write_percentage, "www,w", "write percent");
  }
};
int config::write_percentage = 10;

behavior dictionary_fun(stateful_actor<config::data_map>* self) {
  return {
    [=](write_msg& write_message) {
      auto& key = write_message.key;
      auto& value = write_message.value;
      self->state[key] = value;
      auto& sender = write_message.sender;
      self->send(sender, result_msg{actor_cast<actor>(self), value});
    },
    [=](read_msg& read_message) {
      auto it = self->state.find(read_message.key);
      auto& sender = read_message.sender;
      if (it != end(self->state)) {
        self->send(sender, result_msg{actor_cast<actor>(self), it->second});
      } else {
        self->send(sender, result_msg{actor_cast<actor>(self), -1});
      }
    },
    [=](end_work_msg_atom) {
      cout << "Dictionary Size: " << self->state.size() << endl;
      self->quit();
    }
  };
}

behavior worker_fun(event_based_actor* self, actor master, actor dictionary,
                      int id, int num_msgs_per_worker) {
  const auto write_percent = config::write_percentage;
  int message_count = 0;
  pseudo_random random(id + num_msgs_per_worker + write_percent);
  return {
    [=](result_msg&) mutable {
      ++message_count;
      if (message_count <= num_msgs_per_worker) {
        int an_int = random.next_int(100);
        if (an_int < write_percent) {
          self->send(dictionary,
                     write_msg{actor_cast<actor>(self), random.next_int(),
                               random.next_int()});
        } else {
          self->send(dictionary,
                     read_msg{actor_cast<actor>(self), random.next_int()});
        }
      } else {
        self->send(master, end_work_msg_atom::value);
        self->quit();
      }
    }
  };
}

behavior master_fun(event_based_actor* self, int num_workers,
                    int num_msgs_per_worker) {
  vector<actor> workers;
  workers.reserve(num_workers);
  auto dictionary = self->spawn(dictionary_fun);
  for (int i = 0; i < num_workers; ++i) {
    workers.emplace_back(self->spawn(worker_fun, actor_cast<actor>(self),
                                     dictionary, i, num_msgs_per_worker));
    self->send(workers[i], do_work_msg);
  }
  int num_worker_terminated = 0;
  return {
    [=](end_work_msg_atom) mutable {
      ++num_worker_terminated;
      if (num_worker_terminated == num_workers) {
        self->send(dictionary, end_work_msg_atom::value);
        self->quit();
      }
    },
  };
}

void caf_main(actor_system& system, const config& cfg) {
  int num_workers = cfg.num_entitites;
  int num_msgs_per_worker = cfg.num_msgs_per_worker;
  auto master = system.spawn(master_fun, num_workers, num_msgs_per_worker);
}

CAF_MAIN()

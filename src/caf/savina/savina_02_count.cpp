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

#include "caf/all.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  static int n; //= 1e6;

  config() {
    opt_group{custom_options_, "global"}
    .add(n, "num,n", "number of messages");
  }
};
int config::n = 1e6;

using increment_atom = atom_constant<atom("increment")>;

struct retrieve_msg {
  actor sender;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(retrieve_msg);

struct result_msg {
  int result;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(result_msg);

behavior producer_actor(event_based_actor* self, actor counting) {
  return {
    [=](increment_atom) {
      for (int i = 0; i < config::n; ++i) {
        self->send(counting, increment_atom::value);
      }
      self->send(counting, retrieve_msg{actor_cast<actor>(self)});
    },
    [=](result_msg& m) {
      auto result = m.result;
      if (result != config::n) {
        cout << "ERROR: expected: " << config::n << ", found: " << result
             << endl;
      } else {
        cout << "SUCCESS! received: " << result << endl;
      }
    }
  };
}

behavior counting_actor(stateful_actor<int>* self) {
  self->state = 0;
  return {
    [=](increment_atom) { 
      ++self->state; 
    },
    [=](retrieve_msg& m) {
      self->send(m.sender, result_msg{self->state});
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  auto counting = system.spawn(counting_actor);
  auto producer = system.spawn(producer_actor, counting);
  anon_send(producer, increment_atom::value);
}

CAF_MAIN()

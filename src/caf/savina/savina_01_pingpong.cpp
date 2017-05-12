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

using start_msg_atom = atom_constant<atom("start")>;
using ping_msg_atom = atom_constant<atom("ping")>;
using stop_msg_atom = atom_constant<atom("stop")>;

struct send_ping_msg {
  actor sender;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(send_ping_msg);

struct send_pong_msg {
  actor sender;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(send_pong_msg);

behavior ping_actor(stateful_actor<int>* self, int count, actor pong) {
  self->state = count;
  return {
    [=](start_msg_atom) {
        self->send(pong, send_ping_msg{actor_cast<actor>(self)});
        --self->state;
    },
    [=](ping_msg_atom) {
      self->send(pong, send_ping_msg{actor_cast<actor>(self)});
      --self->state;
    },
    [=](send_pong_msg&) {
      if (self->state > 0) {
        self->send(self, ping_msg_atom::value);
      } else {
        self->send(pong, stop_msg_atom::value);
        self->quit();
      }
    }
  };
}

behavior pong_actor(stateful_actor<int>* self) {
  self->state = 0;
  return {
    [=](send_ping_msg& msg) { 
      auto& sender = msg.sender;
      self->send(sender, send_pong_msg{actor_cast<actor>(self)}); 
      ++self->state;
    },
    [=](stop_msg_atom) { 
      self->quit(); 
    }
  };
}

class config : public actor_system_config {
public:
  int n = 40000;

  config() {
    opt_group{custom_options_, "global"}.add(n, "num,n", "number of ping-pongs");
  }
};

void starter_actor(event_based_actor* self, const config* cfg) {
  auto pong = self->spawn(pong_actor);
  auto ping = self->spawn(ping_actor, cfg->n, pong);
  self->send(ping, start_msg_atom::value);
}

void caf_main(actor_system& system, const config& cfg) {
  system.spawn(starter_actor, &cfg);
}

CAF_MAIN()

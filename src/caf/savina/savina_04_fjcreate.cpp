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
#include <stdlib.h>

#include "caf/all.hpp"

using namespace std;
using namespace caf;

void perform_computation(double theta) {
  double sint = sin(theta);
  double res = sint * sint;
  if (res <= 0) {
    throw string("Benchmark exited with unrealistic res value " + to_string(res));
  }
}

struct message_t {
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(message_t);

behavior fork_join_actor(event_based_actor* self) {
  return {
    [=](message_t) {
      perform_computation(37.2);
      self->quit(); 
    }
  };
}

class config : public actor_system_config {
public:
  int n = 40000;

  config() {
    opt_group{custom_options_, "global"}
    .add(n, "nnn,n", "num of actors");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  message_t message;
  for (int i = 0; i < cfg.n; ++i) {
    auto fj_runner = system.spawn(fork_join_actor);
    anon_send(fj_runner, message);
  }
}

CAF_MAIN()

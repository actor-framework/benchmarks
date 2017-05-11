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
  int n = 25;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of fib numbers");
  }
};

struct request {
  int n;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(request);

struct response {
  int value;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(response);

constexpr auto respone_one = response{1};

struct fibonacci_actor_state {
  int result;
  int resp_received;
};

behavior fibonacci_actor_fun(stateful_actor<fibonacci_actor_state>* self, actor parent) {
  auto& s = self->state;
  s.result = 0;
  s. resp_received = 0;
  auto process_result = [=](const response& resp) {
    auto& s = self->state;
    if (parent) {
      self->send(parent, resp);
    } else {
      cout << " Result = " << s.result << endl;
    }
    self->quit();
  };
  return {
    [=](request& req) {
      auto& s = self->state;
      if (req.n <= 2) {
        s.result = 1;
        process_result(respone_one);
      } else {
        auto f1 = self->spawn(fibonacci_actor_fun, actor_cast<actor>(self));
        self->send(f1, request{req.n - 1});
        auto f2 = self->spawn(fibonacci_actor_fun, actor_cast<actor>(self));
        self->send(f2, request{req.n - 2});
      }
    },
    [=](response& resp) {
      auto& s = self->state;
      ++s.resp_received;  
      s.result += resp.value;
      if (s.resp_received == 2) {
        process_result(response{s.result});
      }
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  auto fj_runner = system.spawn(fibonacci_actor_fun, actor());
  anon_send(fj_runner, request{cfg.n});
}

CAF_MAIN()

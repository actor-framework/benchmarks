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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using msg_atom = atom_constant<atom("msg")>;

namespace {

class receiver : public event_based_actor {
 public:
  receiver(actor_config& cfg, uint64_t max)
      : event_based_actor(cfg),
        max_(max),
        value_(0) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](msg_atom) {
        if (++value_ == max_)
          quit();
      }
    };
  }

 private:
  uint64_t max_;
  uint64_t value_;
};


void sender(actor whom, uint64_t count) {
  auto msg = make_message(msg_atom::value);
  for (uint64_t i = 0; i < count; ++i)
    anon_send(whom, msg);
}

int usage() {
  return cout << "usage: mailbox_performance NUM_THREADS MSGS_PER_THREAD"
              << endl << endl, 1;
}

void run(int argc, char** argv, uint64_t num_sender, uint64_t num_msgs) {
  auto total = num_sender * num_msgs;
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  auto testee = system.spawn<receiver>(total);
  for (uint64_t i = 0; i < num_sender; ++i)
    system.spawn(sender, testee, num_msgs);
}

} // namespace <anonymous>

int main(int argc, char** argv) {
  if (argc != 3)
    return usage();
  run(argc, argv, static_cast<uint64_t>(stoll(argv[1])),
      static_cast<uint64_t>(stoll(argv[2])));
}

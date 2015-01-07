/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

class receiver : public event_based_actor {
 public:
    receiver(uint64_t max) : m_max(max), m_value(0) {
      // nop
    }
    virtual ~receiver();
    behavior make_behavior() override {
      return {
        on(atom("msg")) >> [=] {
          if (++m_value == m_max) {
            quit();
          }
        }
      };
    }
 private:
  uint64_t m_max;
  uint64_t m_value;
};

receiver::~receiver() {
  // nop
}

void sender(actor whom, uint64_t count) {
  if (!whom) return;
  auto msg = make_message(atom("msg"));
  for (uint64_t i = 0; i < count; ++i) {
    anon_send_tuple(whom, msg);
  }
}

void usage() {
    cout << "usage: mailbox_performance NUM_THREADS MSGS_PER_THREAD"
         << endl << endl;
    exit(1);
}

void run(uint64_t num_sender, uint64_t num_msgs) {
    auto total = num_sender * num_msgs;
    auto testee = spawn<receiver>(total);
    for (uint64_t i = 0; i < num_sender; ++i) {
        spawn(sender, testee, num_msgs);
    }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    usage();
  }
  try {
    run(static_cast<uint64_t>(stoll(argv[1])),
        static_cast<uint64_t>(stoll(argv[2])));
  }
  catch (std::exception&) {
    usage();
  }
  await_all_actors_done();
  shutdown();
}

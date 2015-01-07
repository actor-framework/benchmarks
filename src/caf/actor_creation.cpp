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

namespace { uint32_t s_num; }

behavior testee(event_based_actor* self, actor parent) {
  return {
    on(atom("spread"), uint32_t{1}) >> [=] {
      self->send(parent, atom("result"), uint32_t{1});
      self->quit();
    },
    on(atom("spread"), arg_match) >> [=](uint32_t x) {
      auto msg = make_message(atom("spread"), x - 1);
      self->send_tuple(self->spawn(testee, self), msg);
      self->send_tuple(self->spawn(testee, self), msg);
      self->become (
        on(atom("result"), arg_match) >> [=](uint32_t r1) {
          self->become (
            on(atom("result"), arg_match) >> [=](uint32_t r2) {
              if (parent == invalid_actor) {
                uint32_t res = 2 + r1 + r2;
                uint32_t expected = (1 << s_num);
                if (res != expected) {
                  cerr << "expected: " << expected
                       << ", found: " << res
                       << endl;
                  exit(42);
                }
              } else {
                self->send(parent, atom("result"), 1 + r1 + r2);
              }
              self->quit();
            }
          );
        }
      );
    }
  };
}

void usage() {
  cout << "usage: actor_creation POW" << endl
       << "       creates 2^POW actors" << endl << endl;
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    usage();
  }
  try {
    s_num = static_cast<uint32_t>(std::stoi(argv[1]));
  }
  catch (std::exception&) {
    cerr << "invalid argument: " << argv[1];
    usage();
  }
  anon_send(spawn(testee, invalid_actor), atom("spread"), s_num);
  await_all_actors_done();
  shutdown();
}

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

namespace {

uint32_t s_num;

using spread_atom = atom_constant<atom("spread")>;
using result_atom = atom_constant<atom("result")>;

} // namespace <anonymous>

behavior testee(event_based_actor* self, actor parent) {
  return {
    [=](spread_atom, uint32_t x) {
      if (x == 1) {
        self->send(parent, result_atom::value, uint32_t{1});
        self->quit();
        return;
      }
      auto msg = make_message(spread_atom::value, x - 1);
      self->send(self->spawn<lazy_init>(testee, self), msg);
      self->send(self->spawn<lazy_init>(testee, self), msg);
      self->become (
        [=](result_atom, uint32_t r1) {
          self->become (
            [=](result_atom, uint32_t r2) {
              /*if (parent == invalid_actor) {
                uint32_t res = 2 + r1 + r2;
                uint32_t expected = (1 << s_num);
                if (res != expected) {
                  cerr << "expected: " << expected
                       << ", found: " << res
                       << endl;
                  exit(42);
                }
              } else {
                self->send(parent, result_atom::value, 1 + r1 + r2);
              }
              */
              self->send(parent, result_atom::value, 1 + r1 + r2);
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
  if (argc != 2)
    usage();
  s_num = static_cast<uint32_t>(std::stoi(argv[1]));
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  scoped_actor self{system};
  anon_send(system.spawn<lazy_init>(testee, self), spread_atom::value, s_num);
}

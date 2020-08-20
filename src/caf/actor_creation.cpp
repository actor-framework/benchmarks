/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

// TODO: use proper version check once 0.18 has an official release
#ifdef CAF_BEGIN_TYPE_ID_BLOCK

CAF_BEGIN_TYPE_ID_BLOCK(actor_creation, first_custom_type_id)

  CAF_ADD_ATOM(actor_creation, spread_atom);
  CAF_ADD_ATOM(actor_creation, result_atom);

CAF_END_TYPE_ID_BLOCK(actor_creation)

#else // CAF_BEGIN_TYPE_ID_BLOCK

using spread_atom = caf::atom_constant<caf::atom("spread")>;
using result_atom = caf::atom_constant<caf::atom("result")>;
static constexpr spread_atom spread_atom_v = spread_atom::value;
static constexpr result_atom result_atom_v = result_atom::value;

#endif // CAF_BEGIN_TYPE_ID_BLOCK

using namespace caf;

namespace {

uint32_t s_num;

} // namespace <anonymous>

behavior testee(event_based_actor* self, actor parent) {
  return {
    [=](spread_atom, uint32_t x) {
      if (x == 1) {
        self->send(parent, result_atom_v, uint32_t{1});
        self->quit();
        return;
      }
      auto msg = make_message(spread_atom_v, x - 1);
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
                self->send(parent, result_atom_v, 1 + r1 + r2);
              }
              */
              self->send(parent, result_atom_v, 1 + r1 + r2);
              self->quit();
            }
          );
        }
      );
    },
  };
}

void usage() {
  std::cout << "usage: actor_creation POW\n"
               "       creates 2^POW actors\n\n";
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2)
    usage();
#ifdef CAF_BEGIN_TYPE_ID_BLOCK
  init_global_meta_objects<caf::id_block::actor_creation>();
#endif
  s_num = static_cast<uint32_t>(std::stoi(argv[1]));
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  scoped_actor self{system};
  anon_send(system.spawn<lazy_init>(testee, self), spread_atom_v, s_num);
}

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

#include <caf/actor_system.hpp>
#include <caf/actor_system_config.hpp>
#include <caf/anon_mail.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/init_global_meta_objects.hpp>
#include <caf/message.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/type_id.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

CAF_BEGIN_TYPE_ID_BLOCK(actor_creation, first_custom_type_id)

  CAF_ADD_ATOM(actor_creation, spread_atom);
  CAF_ADD_ATOM(actor_creation, result_atom);

CAF_END_TYPE_ID_BLOCK(actor_creation)

namespace {

uint32_t s_num;

} // namespace

caf::behavior testee(caf::event_based_actor* self, caf::actor parent) {
  return {
    [=](spread_atom, uint32_t x) {
      if (x == 1) {
        self->mail(result_atom_v, uint32_t{1}).send(parent);
        self->quit();
        return;
      }
      auto msg = caf::make_message(spread_atom_v, x - 1);
      self->mail(msg).send(self->spawn<caf::lazy_init>(testee, self));
      self->mail(msg).send(self->spawn<caf::lazy_init>(testee, self));
      self->become([=](result_atom, uint32_t r1) {
        self->become([=](result_atom, uint32_t r2) {
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
            self->mail(result_atom_v, 1 + r1 + r2).send(parent);
          }
          */
          self->mail(result_atom_v, 1 + r1 + r2).send(parent);
          self->quit();
        });
      });
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
  caf::init_global_meta_objects<caf::id_block::actor_creation>();
  caf::core::init_global_meta_objects();
  s_num = static_cast<uint32_t>(std::stoi(argv[1]));
  caf::actor_system_config cfg;
  caf::actor_system system{cfg};
  caf::scoped_actor self{system};
  caf::anon_mail(spread_atom_v, s_num)
    .send(system.spawn<caf::lazy_init>(testee, self));
}

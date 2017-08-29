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

#include "benchmark_helper.hpp"

using namespace std;
using namespace caf;

using done_atom = atom_constant<atom("done")>;

struct bang_t {
  std::array<uint64_t, 5> refs;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bang_t);

void send_fun(event_based_actor* self, actor t, bang_t m, int n) {
  for (int i = 0; i < n; ++i) {
    self->send(t, m);
  }
}

behavior rec_fun(event_based_actor* self, actor parent, int n) {
  return {
    [=] (const bang_t&) mutable {
      --n;
      if (n == 0) {
        self->send(parent, done_atom::value);
        self->quit(); //oK;
      }
    }
  };
}

behavior run(event_based_actor* self, int s, int m) {
  bang_t bang;
  auto rec = spawn_link(self, rec_fun, actor_cast<actor>(self), s * m);
  for (int i = 0; i < s; ++i) {
    spawn_link(self, send_fun, rec, bang, m);
  }
  return {
    [=](done_atom) {
      // receive Done -> ok end,
      self->quit();
    }
  };
}

void usage() {
  cout << "usage: bencherl_01_bang VERSION NUM_CORES" << endl
       << "       VERSION:      short|intermediate|long " << endl
       << "       NUM_CORES:    number of cores" << endl << endl
       << "  for details see http://release.softlab.ntua.gr/bencherl/" << endl;
  exit(1);
}

int main(int argc, char** argv) {
  // configuration
  if (argc != 3)
    usage();
  string version = argv[1];
  int f;
  if (version == "test") {
    f = 1;
  } else if (version == "short") {
    f = 16;
  } else if (version == "intermediate") {
    f = 55;
  } else if (version == "long") {
    f = 79;
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  int s = f * cores; //num of senders 
  int m = f * cores; //num of messages
  system.spawn(run, s, m);
  system.await_all_actors_done();
}


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

struct bang_t {
  std::array<uint64_t, 5> refs;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bang_t);

void send_fun(event_based_actor* self, actor t, bang_t m, int n) {
  for (int i = 0; i < n; ++i) {
    self->send(t, m);
  }
}

behavior rec_fun(event_based_actor*, int n) {
  return {
    [=] (const bang_t&) mutable {
      --n;
      //if (n == 0) {
        //self->quit();
      //}
    }
  };
}

void run(actor_system& system, int s, int m) {
  bang_t bang;
  auto rec = system.spawn(rec_fun, s * m);
  for (int i = 0; i < s; ++i) {
    system.spawn(send_fun, rec, bang, m); 
  }
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
  run(system, s, m);
}


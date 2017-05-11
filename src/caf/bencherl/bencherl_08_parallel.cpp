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
#include <chrono>
#include <bitset>

#include "caf/all.hpp"

#include "erlang_pattern_matching.hpp"

using namespace std;
using namespace caf;

using done_atom = atom_constant<atom("done")>;

using time_vector_type = vector<chrono::time_point<chrono::system_clock>>;

bool check_now(time_vector_type& ts) {
  //check_now([_,_]) -> ok;
  //check_now([_|Ts]) -> check_now(Ts).
  auto last = --end(ts); 
  for (auto t = begin(ts);; ++t) {
    if (t == last) {
      return true; 
    } 
  }
  return false;
}

void loop(event_based_actor* self, actor pid, size_t n) {
  //loop(Pid, 0, Out) -> Pid ! {self(), check_now(Out)};
  //loop(Pid, N, Out) -> loop(Pid, N - 1, [now()|Out]).
  time_vector_type out;
  out.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    out.emplace_back(chrono::system_clock::now());
  }
  self->send(pid, check_now(out));
}

bool run(actor_system& system, size_t n, size_t m) {
	//Me   = self(),
	//Base = [ok || _ <- lists:seq(1, M)],
	//Pids = [spawn_link(fun() -> loop(Me, N, []) end) || _ <- lists:seq(1, M)],
	//Res  = [receive {Pid, What} -> What end || Pid <- Pids],
	//Base = Res,
	//ok.
  scoped_actor self{system};
  auto& me = self;
  vector<bool> base(m, true);
  erlang_pattern_matching<actor> pids;
  for (size_t i = 0; i < m; ++i) {
    pids.match_list().emplace_back(system.spawn(loop, me, n));
  }
  vector<bool> res(m, false);
  size_t i = 0;
  pids.restart();
  while(!pids.matched()) {
    self->receive([&] (bool what) {
      if (i < res.size()) {
        res[i] = what; 
        ++i;
      }
      pids.match(actor_cast<actor>(self->current_sender()));
    });
  }
  if (std::equal(begin(base), end(base), begin(res))) {
    return true;    
  } else {
    return false; 
  }
}

void usage() {
  cout << "usage: bencherl_08_parallel VERSION NUM_CORES" << endl
       << "       VERSION:      short|intermediate|long " << endl
       << "       NUM_CORES:    number of cores" << endl << endl
       << "  for details see http://release.softlab.ntua.gr/bencherl/" << endl;
  exit(1);
}

int main(int argc, char** argv) {
  //configuration
  if (argc != 3)
    usage();
  string version = argv[1];
  std::tuple<int, int> f;
  //short -> [313, 4];
  //intermediate -> [469, 8];
  //long -> [1094, 10]
  if (version == "test") {
    f = make_tuple(1,1);
  } else if (version == "short") {
    f = make_tuple(313, 4);
  } else if (version == "intermediate") {
    f = make_tuple(469, 8);
  } else if (version == "long") {
    f = make_tuple(1094, 10);
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  auto n = get<1>(f) * cores;
  auto m = get<0>(f) * cores;
  run(system, n, m);
}


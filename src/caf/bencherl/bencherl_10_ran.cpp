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

using go_atom = atom_constant<atom("go")>;
using rand_t = int;
using Acc_t = vector<rand_t>;

Acc_t mk_ranlist(size_t len, size_t max) {
  //mk_ranlist(0, _, Acc) -> Acc;
  //mk_ranlist(N, M, Acc) -> mk_ranlist(N-1, M, [random:uniform(M) | Acc]). 

  //mk_ranlist(Len, Max) ->
    //random:seed(Len, Max, Max * 2),
    //mk_ranlist(Len, Max, []).
  
  mt19937 rand_engine(len + max + max * 2);
  uniform_int_distribution<> unif(1, max);
  Acc_t acc;
  acc.reserve(len);
  for (size_t i = 0; i < len; ++i) {
     acc.emplace_back(unif(rand_engine));
  }
  return acc;
}

rand_t random(size_t n) {
	//Len = 100000,
  size_t len = 100000;
	//{_, [Mid| _]} = lists:split(Len div 2, lists:sort(mk_ranlist(Len, 2*N))),
  auto tmp_list = mk_ranlist(len, 2*n);
  sort(begin(tmp_list), end(tmp_list)); 
  return tmp_list[len / 2];
}

void run(actor_system& system, size_t n) {
  scoped_actor gself{system}; 
	//Parent = self(),
  actor parent = gself;
	//PList = lists:map(fun (_) ->
		//spawn(fun () ->
			//receive {Parent, go} -> ok end,
			//Parent ! {self(), random(100)}
		//end)
	//end, lists:seq(1,N)),
  erlang_pattern_matching<actor> plist;
  for (size_t i = 0; i < n; ++i) {
    plist.match_list().emplace_back(system.spawn([=](event_based_actor* self) {
      self->become([=](go_atom) {
        self->send(parent, random(100)); 
      });
    }));
  }
	//lists:foreach(fun (P) -> P ! {Parent, go} end, PList),
  for (auto& p : plist.match_list()) {
    gself->send(p, go_atom::value); 
  }
	//lists:foreach(fun (P) -> receive {P, _RN} -> ok end end, PList),
  plist.restart();
  while(!plist.matched()){
    gself->receive([&](rand_t){
      plist.match(actor_cast<actor>(gself->current_sender()));
    }); 
  }
}

void usage() {
  cout << "usage: bencherl_10_ran VERSION NUM_CORES" << endl
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
  int f;
  //short -> 1;
  //intermediate -> 2;
  //long -> 4
  if (version == "test") {
    f = 1;
  } else if (version == "short") {
    f =  1;
  } else if (version == "intermediate") {
    f = 2;
  } else if (version == "long") {
    f = 4;
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  auto n = f * cores;
  run(system, n);
}


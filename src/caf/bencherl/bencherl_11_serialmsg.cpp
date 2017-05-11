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

using do_atom = atom_constant<atom("do")>;
using done_atom = atom_constant<atom("done")>;

using recvs_t = erlang_pattern_matching<actor>;
using gens_t = vector<actor>;

struct data_t {
  vector<uint64_t> d;
};

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(data_t);

behavior receiver(event_based_actor* self, actor master) {
	//receive
		//{_, done} -> Master ! {self(), done};
		//{_, _} -> receiver(Master)
	//end.
  return  {
    [=] (done_atom done) {
      self->send(master, done);
    },
    [=] (actor /*pid*/, data_t&) {
    
    }
  };
}

behavior dispatcher(event_based_actor* self, actor master) {
	//receive
		//{Master, done} -> ok;
		//{Pid, To, Data} -> 	To ! {Pid, Data},
							//dispatcher(Master)
	//end.
  return  {
    [=] (done_atom) {
      if (master == self->current_sender()) {
        self->quit(); 
      }
    },
    [=] (actor to, data_t& data) {
      self->send(to, actor_cast<actor>(self->current_sender()), move(data));
    }
  };
}

behavior generator(event_based_actor* self, actor recv, actor disp,
                   actor master, size_t n, size_t l) {
  return {
    //receive
      //{Master, do} -> generator_push_loop(Recv, Disp, N, Data);
    //end.
    [=] (do_atom) {
      if (master == self->current_sender()) {
        //Data = lists:seq(1, L),
        data_t data;
        data.d.reserve(l);
        for (size_t i = 0; i < l; ++i) {
          data.d.emplace_back(i);
        }
        //generator_push_loop(Recv, Disp, 0, _) ->
          //Disp ! {self(), Recv, done};
        //generator_push_loop(Recv, Disp, N, Data) ->
          //Disp ! {self(), Recv, Data},
          //generator_push_loop(Recv, Disp, N - 1, Data).
        for (size_t i = 0; i < n; ++i) {
          self->send(disp, recv, data);
        }
        self->send(recv, done_atom::value);
      }
    }
  };
}



recvs_t setup_receivers(scoped_actor& self, size_t p) {
  //setup_receivers(P) -> setup_receivers(P, self(), []).
  //setup_receivers(0, _, Out) -> Out;
  //setup_receivers(P, Pid, Out) -> 
    //setup_receivers(P - 1, Pid, [spawn_link(fun() -> receiver(Pid) end)|Out]).
  recvs_t result; 
  result.match_list().reserve(p);
  for (size_t i = 0; i < p; ++i) {
    result.match_list().emplace_back(
      self->spawn(receiver, actor_cast<actor>(self)));
  }
  return result;
}

actor setup_dispatcher(scoped_actor& self) {
	//Me = self(),
	//spawn_link(fun() -> dispatcher(Me) end).
  actor me = self;
  return self->spawn(dispatcher, me);
}

gens_t setup_generators(scoped_actor& self, recvs_t& recvs, actor disp, size_t n,
                        size_t l) {
  //setup_generators(Recvs, Disp, N, L) ->
    //setup_generators(Recvs, Disp, self(), N, L, []).

  //setup_generators([],_,  _, _, _, Out) -> Out;
  //setup_generators([Recv|Recvs], Disp, Pid, N, L, Out) ->
    //setup_generators(Recvs, Disp, Pid, N, L, [spawn_link(fun() -> generator(Recv, Disp, Pid, N, L) end) | Out]).
  gens_t out;
  out.reserve(recvs.match_list().size());
  for (auto& recv : recvs.match_list()) {
    out.emplace_back(
      self->spawn(generator, recv, disp, actor_cast<actor>(self), n, l));
  } 
  return out;
}

void run(actor_system& system, size_t p, size_t n, size_t l) {
  scoped_actor self{system};
	//Recvs = setup_receivers(P),
  auto recvs = setup_receivers(self, p);
	//Disp  = setup_dispatcher(),
  auto disp = setup_dispatcher(self);
	//Gens  = setup_generators(Recvs, Disp, N, L),
  auto gens = setup_generators(self, recvs, disp, n, l);
	//[Pid ! {self(), do} || Pid <- Gens],
  for (auto& gen : gens) {
    self->send(gen, do_atom::value); 
  }
	//[receive {Pid, done} -> ok end || Pid <- Recvs],
  recvs.restart();
  while(!recvs.matched()) {
    self->receive([&](done_atom){
      recvs.match(actor_cast<actor>(self->current_sender()));
    });
  }
	//Disp ! {self(), done},
  self->send(disp, done_atom::value);
}

void usage() {
  cout << "usage: bencherl_11_serialmsg VERSION NUM_CORES" << endl
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
  std::tuple<int, int, int> f;
  //short -> [2, 16, 32];  
  //intermediate -> [2, 32, 40];  
  //long -> [5, 16, 32]
  if (version == "test") {
    f = make_tuple(1, 1, 1);
  } else if (version == "short") {
    f = make_tuple(2, 16, 32);
  } else if (version == "intermediate") {
    f = make_tuple(2, 32, 40);
  } else if (version == "long") {
    f = make_tuple(5, 16, 32);
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  size_t p = std::get<0>(f) * cores;
  size_t n = std::get<1>(f) * cores;
  size_t l = std::get<2>(f) * cores;
  run(system, p, n, l);
}


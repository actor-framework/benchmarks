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

using init_atom = atom_constant<atom("init")>;
using start_atom = atom_constant<atom("start")>;
using done_atom = atom_constant<atom("done")>;
using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

struct handler_state;
using fun_t = function<void(stateful_actor<handler_state>* self)>;

enum class bench_type {
  wheel,
  no_wheel
};

struct handler_state {
  erlang_pattern_matching<actor> pids;
  vector<actor>::iterator pid_it;
  size_t n;
  fun_t fun;
  actor me;
};

void loop_impl(stateful_actor<handler_state>* self) {
  auto& s = self->state;
  //loop([], 0, _, Me) ->
    //Me ! {self(), done};
  //loop([], N, Fun, Me) ->
    //loop([], Fun(undefined, N), Fun, Me);
  //loop([Pid|Pids], N, Fun, Me) ->
    //Pid ! {self(), ping},
    //loop(Pids, Fun(Pid, N), Fun, Me).
  if (s.n == 0 && s.pid_it == end(s.pids.match_list()) && s.pids.matched()) {
    self->send(s.me, done_atom::value); 
    self->quit();
    return;
  } else if (s.pid_it == end(s.pids.match_list())) {
    s.fun(self);
  } else {
    self->send(*s.pid_it, ping_atom::value);
    ++s.pid_it;
    s.fun(self); 
  }
}

void loop(stateful_actor<handler_state>* self, size_t n, fun_t fun, actor me) {
  auto& s = self->state;
  s.pid_it = begin(s.pids.match_list());
  s.n = n;
  s.fun = move(fun);
  s.me = move(me);
  loop_impl(self);
}

behavior handler(stateful_actor<handler_state>* self, size_t n, fun_t fun, actor me) {
  self->set_default_handler(skip);
  //handler(N, Fun, Me) ->
    //Others = receive {init, Pids} -> Pids end,
    //receive start -> ok end,
    //loop(Others, N, Fun, Me).
  return {
    [=] (init_atom, vector<actor>& others) {
      self->state.pids.foreach(move(others)); 
      self->become([=](start_atom){
        loop(self, n, fun, me); 
      });
    }
  };
}

void recv_loop_after(stateful_actor<handler_state>* self) {
  //recv_loop_after(_, 0) ->
    //0;
  //recv_loop_after(Pid, N) ->
    //receive
      //{Pid, pong} -> N;
      //{Other, ping} ->	Other ! {self(), pong},
                //recv_loop_after(Pid, N - 1)
      //after 1073741824 -> exit(self(), kill)
    //end.
  if (self->state.n == 0 && self->state.pids.matched()) {
    loop_impl(self);  
    return;
  } else {
    self->become(
      [=](pong_atom){
        if (self->state.pids.match(actor_cast<actor>(self->current_sender()))){
        }
        loop_impl(self);
      },
      [=](ping_atom){
        --self->state.n;
        self->send(actor_cast<actor>(self->current_sender()), pong_atom::value);
        recv_loop_after(self);
      },
      after(std::chrono::milliseconds(1073741824)) >> [=] {
        // should never happed
        cerr << "Error: Timeout reached" << endl;
        exit(1);
    });
  }
}

void recv_loop(stateful_actor<handler_state>* self) {
  //recv_loop(_, 0) ->
    //0;
  //recv_loop(Pid, N) ->
    //receive
      //{Pid, pong} -> N;
      //{Other, ping} ->	Other ! {self(), pong},
                //recv_loop(Pid, N - 1)
    //end.
  if (self->state.n == 0 && self->state.pids.matched()) {
    loop_impl(self);  
    return;
  } else {
    self->become(
      [=](pong_atom){
        self->state.pids.match(actor_cast<actor>(self->current_sender()));
        loop_impl(self);
      },
      [=](ping_atom){
        --self->state.n;
        self->send(actor_cast<actor>(self->current_sender()), pong_atom::value);
        recv_loop(self);
    });
  }
}

void test(actor_system& system, size_t n, fun_t fun) {
  scoped_actor self{system};
	//Me = self(),
  actor me = self;
	//Pids = [spawn_link(fun() -> handler(N - 1, Fun, Me) end) || _ <- lists:seq(1, N)],
  erlang_pattern_matching<actor> pids;
  for (size_t i = 0; i < n; ++i) {
    pids.match_list().emplace_back(system.spawn(handler, n - 1, fun, me));
  }
	//[Pid ! {init, Pids -- [Pid]} || Pid <- Pids],
  for (auto& pid : pids.match_list()) {
    vector<actor> tmp;
    tmp.reserve(pids.match_list().size() - 1);
    copy_if(begin(pids.match_list()), end(pids.match_list()), back_inserter(tmp), [=] (const actor& a) {
        return a != pid;
    });
    self->send(pid, init_atom::value, std::move(tmp));
  }
	//[Pid ! start || Pid <- Pids],
  for (auto& pid : pids.match_list()) {
    self->send(pid, start_atom::value);
  }
	//[receive {Pid, done} -> ok end || Pid <- Pids],
  pids.restart();
  while(!pids.matched()) {
    self->receive([&](done_atom){
      pids.match(actor_cast<actor>(self->current_sender()));
    }); 
  }
}

void run(actor_system& system, bench_type t, size_t n) {
  //run([wheel,N|_], _, _) ->
    //test(N, fun recv_loop_after/2);
  //run([no_wheel,N|_], _, _) ->
    //test(N, fun recv_loop/2).
  if (t == bench_type::wheel) {
    test(system, n, recv_loop_after);
  } else { // t == bench_type::no_wheel
    test(system, n, recv_loop);
  }
}

void usage() {
  cout << "usage: bencherl_12_timer_wheel VERSION NUM_CORES BENCH_TYPE" << endl
       << "       VERSION:      short|intermediate|long " << endl
       << "       NUM_CORES:    number of cores" << endl 
       << "       BENCH_TYPE:   <wheel|no_wheel>" << endl << endl
       << "  for details see http://release.softlab.ntua.gr/bencherl/" << endl;
  exit(1);
}

int main(int argc, char** argv) {
  //configuration
  if (argc != 4)
    usage();
  string version = argv[1];
  string type = argv[3];
  int f;
  //short -> 16;
  //intermediate -> 40;
  //long -> 125
  if (version == "test") {
    f = 1;
  } else if (version == "short") {
    f = 16;
  } else if (version == "intermediate") {
    f = 40;
  } else if (version == "long") {
    f = 125;
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  bench_type t;
  if (type == "wheel") {
    t = bench_type::wheel;   
  } else if (type == "no_wheel") {
    t = bench_type::no_wheel;
  } else {
    std::cerr << "bench type musst be wheel or no_wheel" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  size_t n = f * cores;
  run(system, t, n);
}


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

using proc_call_atom = atom_constant<atom("proc_call")>;
using stop_atom = atom_constant<atom("stop")>;
using stress_atom = atom_constant<atom("stress")>;
using ok_atom = atom_constant<atom("ok")>;
using dont_match_me_atom = atom_constant<atom("dontmatch")>;

struct client_state {
 size_t n;
 actor serv;
 actor pid;
};

void client_impl(stateful_actor<client_state>* self, proc_call_atom calltype);
void client_call(stateful_actor<client_state>* self, proc_call_atom calltype,
                 actor s, stress_atom msg);
void client(stateful_actor<client_state>* self, proc_call_atom calltype,
            actor serv, size_t n, actor pid);

behavior client(stateful_actor<client_state>* self, size_t cqueue) {
  self->set_default_handler(skip);
	//[self() ! dont_match_me || _ <- lists:seq(1, Queue)],
	//client().
  for (size_t i = 0; i < cqueue; ++i) {
    self->send(actor_cast<actor>(self), dont_match_me_atom::value);
  }
  return {
		//{{proc_call, S}, N, Pid} -> client({proc_call,S}, N, Pid)
    [=] (proc_call_atom type, actor serv, size_t n) {
      client(self, type, serv, n, actor_cast<actor>(self->current_sender()));
    } 
  };
}
void client(stateful_actor<client_state>* self, proc_call_atom calltype,
            actor serv, size_t n, actor pid) {
  auto& s = self->state; 
  s.n = n; 
  s.serv = serv;
  s.pid = pid;
  client_impl(self, calltype);
}
void client_impl(stateful_actor<client_state>* self, proc_call_atom calltype) {
  auto& s = self->state;
  if (s.n == 0) {
    //client(_, 0, Pid) -> Pid ! {self(), ok};
    self->send(s.pid, ok_atom::value);
    self->quit();
    return;
  } else {
		//stress = client_call(CallType, stress),
	  //client(CallType, N - 1, Pid).
    --s.n;
    client_call(self, calltype, s.serv, stress_atom::value);
    return;
  }
}
void client_call(stateful_actor<client_state>* self, proc_call_atom calltype,
                 actor s, stress_atom msg) {
  //client_call({proc_call,S}, Msg) -> S ! {self(), Msg}, receive {S,Ans} -> Ans end.
  self->send(s, msg);
  self->become([=] (stress_atom) {
    client_impl(self, calltype);     
  });
}

behavior server(event_based_actor* self) {
	//receive 
		//stop -> ok;
		//{From, Msg} -> From ! {self(), Msg}, server()
	//end.
  return {
    [=] (stop_atom) {
      self->quit(); 
    },
    [=] (stress_atom type) {
      return type;
    }
  };
}

actor start_server(actor_system& system, proc_call_atom) {
  return system.spawn(server);
}

void stop_server(actor server, proc_call_atom) {
  anon_send(server, stop_atom::value); 
}

erlang_pattern_matching<actor> start_clients(actor_system& system, size_t np, size_t cqueue) {
  erlang_pattern_matching<actor> result; 
  for (size_t i = 0; i < np; ++i) {
    //client overload resolution problem
    //result.match_list().emplace_back(system.spawn(client, cqueue)); 
    result.match_list().emplace_back(system.spawn([=](stateful_actor<client_state>* self) {
      return client(self, cqueue);
    }));
  } 
  return result;
}

void stop_clients(erlang_pattern_matching<actor>& /*clients*/) {
  //for (auto& c : clients.match_list()) {
    //c.quit(); 
  //}
}

void run(actor_system& system, proc_call_atom type, size_t np, size_t n, size_t cqueue) {
  scoped_actor self{system};
  auto server = start_server(system, type); 
  auto clients = start_clients(system, np, cqueue);
  for (auto& c : clients.match_list()) {
    self->send(c, type, server, n);
  }
  clients.restart();
  while(!clients.matched()) {
    self->receive([&](ok_atom){
       clients.match(actor_cast<actor>(self->current_sender()));
      }); 
  }
  stop_server(server, type);
  stop_clients(clients);
}

void usage() {
  cout << "usage: bencherl_05_genstress VERSION NUM_CORES" << endl
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
		//short -> [16, 4, 8]; 
		//intermediate -> [16, 10, 11]; 
		//long -> [16, 47, 79]
  if (version == "test") {
    f = make_tuple(1,1,1);
  } else if (version == "short") {
    f = make_tuple(16, 4, 8);
  } else if (version == "intermediate") {
    f = make_tuple(16, 10, 11);
  } else if (version == "long") {
    f = make_tuple(16, 47, 79);
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  proc_call_atom type;
  auto  np = get<0>(f) * cores;
  auto n = get<1>(f) * cores;
  auto cqueue = get<2>(f) * cores;
  run(system, type, np, n, cqueue);
}


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

using ready_atom = atom_constant<atom("ready")>;
using go_atom = atom_constant<atom("go")>;
using done_atom = atom_constant<atom("done")>;
using are_you_keeping_up_atom = atom_constant<atom("areup")>;
using i_am_keeping_up_atom = atom_constant<atom("iamup")>;

//-define(ACK, 100).
//-define(GSIZE, 20).
enum {
  ack = 100,
  gsize = 20
};

//-define(DATA, {a,b,c,d,e,f,g,h,i,j,k,l}). %% 104 bytes on a 64-bit machine
struct data_t {
  std::array<char,104> tmp; 
} data;
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(data_t);

std::string to_string(data_t&) {
  return "data";
}

struct sender_state;
void sender(stateful_actor<sender_state>* self, vector<actor> rs, int loop);
void sender_impl(stateful_actor<sender_state>* self);
void sender_ack(stateful_actor<sender_state>* self, int ack);
void sender_ack_impl(stateful_actor<sender_state>* self);

struct receiver_state {
  int senders_left;
};

behavior receiver(stateful_actor<receiver_state>* self, actor gmaster,
                  int init_senders_left) {
  // for some reason drop is not working, we have to drop msgs manually
  // self->set_default_handler(drop); // receive Msg -> ...  _ -> ok end, 
  auto& senders_left = self->state.senders_left;
  senders_left = init_senders_left;
  return {
    //done -> 
    	//case SendersLeft of
        //1 -> GMaster ! {self(), done};
        //_ -> receiver(GMaster, SendersLeft - 1)
    	//end;
    [=] (done_atom) mutable{
      if (senders_left == 1) {
        self->send(gmaster, done_atom::value);     
      } else {
        --senders_left;
      }
    },
    //Msg -> 
    	//case Msg of
        //{From, are_you_keeping_up} -> From ! {self(), i_am_keeping_up};
        //_ -> ok
    	//end,
    	//receiver(GMaster, SendersLeft)
    [=] (are_you_keeping_up_atom) {
      self->send(actor_cast<actor>(self->current_sender()),
                 i_am_keeping_up_atom::value);
    },
    [=] (const data_t&) {
      // drop 
    }
  };
}

struct sender_state {
  erlang_pattern_matching<actor> rs;
  int sender_loop; //state of sender(...)
  int sender_ack_n; //state of sender_ack(...)
};

void sender_ack(stateful_actor<sender_state>* self, int ack) {
  auto& s = self->state;
  s.sender_ack_n = ack; 
  sender_ack_impl(self);
}

void sender_ack_impl(stateful_actor<sender_state>* self) {
  auto& rs = self->state.rs;
  auto& n = self->state.sender_ack_n;
  if (n == 1) {
    //lists:foreach(fun (R) ->
        //R ! ?DATA,
        //R ! {self(), are_you_keeping_up}
      //end, Rs),
    //lists:foreach(fun (R) ->
        //receive {R, i_am_keeping_up} -> ok end,
        //R ! ?DATA
      //end, Rs),
    //ok;
    for (auto& r : rs.match_list()) {
      self->send(r, data);

      self->send(r, are_you_keeping_up_atom::value);
    }
    rs.restart();
    self->become(
      [=] (i_am_keeping_up_atom) mutable {
        self->send(actor_cast<actor>(self->current_sender()), data);
        if(rs.match(actor_cast<actor>(self->current_sender()))){
          sender_impl(self); 
        }
      });
    return; 
  } else {
    //lists:foreach(fun (R) -> R ! ?DATA end, Rs),
    //sender_ack(Rs, N-1).
    for (auto& r : rs.match_list()) {
      self->send(r, data);
    }
    --n;
  }
  sender_ack_impl(self); // tail recursive
}

void sender(stateful_actor<sender_state>* self, vector<actor> rs, int loop) {
  self->state.rs.foreach(std::move(rs));
  self->state.sender_loop = loop;
  sender_impl(self);
}

void sender_impl(stateful_actor<sender_state>* self) {
  auto& rs = self->state.rs;
  auto& loop = self->state.sender_loop;
  if (loop == 0) {
    //lists:foreach(fun (R) -> R ! done end, Rs);
    for (auto& r : rs.match_list()) {
      self->send(r, done_atom::value); 
    }    
    self->quit();
    return;
  } else if (loop > ack) {
    //sender_ack(Rs, ?ACK),
    //sender(Rs, Loop - ?ACK);
    loop -= ack;
    sender_ack(self, ack);
    // sender_ack calls sender(self) therefore the recurisve loop is not
    // interrupted
    // this is required as sender_ack has to wait for messages in between
    return; 
  } else {
    //lists:foreach(fun (R) -> R ! ?DATA end, Rs),
    //sender(Rs, Loop-1).
    for (auto& r : rs.match_list()) {
       self->send(r, data);
    }
    --loop;
  }
  sender_impl(self); //tail recursive
}

struct group_state {
  erlang_pattern_matching<actor> rs;
  erlang_pattern_matching<actor> ss;
};

actor group_fun(actor_system& system, actor master, int loop) {
  return system.spawn([=] (stateful_actor<group_state>* gself) {
    gself->set_default_handler(skip);
    auto& rs = gself->state.rs;
    auto& ss = gself->state.ss;
    auto gmaster = actor_cast<actor>(gself); 
    //Rs = lists:map(fun (_) ->
        //spawn_link(fun () -> receiver(GMaster, ?GSIZE) end)
        //end, lists:seq(1, ?GSIZE)),
    rs.match_list().reserve(gsize);
    for (int i= 0; i < gsize; ++i) {
      rs.match_list().emplace_back(gself->spawn(receiver, gmaster, gsize)); 
    }
    //Ss = lists:map(fun (_) ->
        //spawn_link(fun () ->
          //receive {GMaster, go} -> sender(Rs,Loop) end
        //end)
      //end, lists:seq(1, ?GSIZE)),
    ss.match_list().reserve(gsize);
    for (int i= 0; i < gsize; ++i) {
      ss.match_list().emplace_back(
        gself->spawn([=](stateful_actor<sender_state>* self) {
          self->set_default_handler(skip);
          self->become({
            [=] (go_atom) {
              if (gmaster == self->current_sender()){
                sender(self, rs.match_list(), loop);
              }
            }
          });
        }));
    }
    //Master ! {self(), ready},
    gself->send(master, ready_atom::value);
    //receive {Master, go} -> ok end,
    gself->become({
      [=] (go_atom) mutable {
        if (master == gself->current_sender()){
          //lists:foreach(fun (S) -> S ! {GMaster, go} end, Ss),
          for (auto& s: ss.match_list()) {
            gself->send(s, go_atom::value);
          }
          //lists:foreach(fun (R) -> receive {R, done} -> ok end end, Rs),
          rs.restart(); 
          gself->become({
            [=] (done_atom) mutable {
              if (rs.match(actor_cast<actor>(gself->current_sender()))) {
                gself->send(master, done_atom::value); 
                gself->quit();
              }
            }
          });
        }
      }
    });
  });
}

void run(actor_system& system, int n, int m) {
  erlang_pattern_matching<actor> erl;
  scoped_actor self{system};
  auto master = actor(self);
  //Gs = lists:map(fun (_) -> group(Master, M) end, lists:seq(1, N)),
  vector<actor> gs;
  gs.reserve(n);
  for (int i = 0; i < n; ++i) {
    gs.emplace_back(group_fun(system, master, m));
  }
  //lists:foreach(fun (G) -> receive {G, ready} -> ok end end, Gs),
  erl.foreach(std::move(gs));
  while(!erl.matched()) {
  self->receive([&] (ready_atom) {
      erl.match(actor_cast<actor>(self->current_sender()));
    });
  }
  //lists:foreach(fun (G) -> G ! {Master, go} end, Gs),
  for (auto& g: erl.match_list()) {
    self->send(g, go_atom::value);
  }
  //lists:foreach(fun (G) -> receive {G, done} -> ok end end, Gs),
  erl.restart();
  while(!erl.matched()) {
    self->receive([&](done_atom){
      erl.match(actor_cast<actor>(self->current_sender()));
    });
  }
}

void usage() {
  cout << "usage: bencherl_03_ehb VERSION NUM_CORES" << endl
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
  int f1;
  int f2;
  if (version == "test") {
    f1 = 1;
    f2 = 101;
  } else if (version == "short") {
    f1 = 1;
    f2 = 4;
  } else if (version == "intermediate") {
    f1 = 2;
    f2 = 8;
  } else if (version == "long") {
    f1 = 8;
    f2 = 8;
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  int n = f1 * cores;
  int m = f2 * cores;
  run(system, n, m); 
}


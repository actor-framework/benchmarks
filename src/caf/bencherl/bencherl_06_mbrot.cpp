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

//-define(MAXITER, 255).
//-define(LIM_SQR, 4.0).
//-define(RL, 2.0).
//-define(IL, 2.0).
static constexpr int maxiter = 255;
static constexpr double lim_sqr = 4.0;
static constexpr double rl = 2.0;
static constexpr double il = 2.0;

// avoids unwanted compiler optimization (slows down the bench)
static volatile int* avoid_opt; 

void mbrot(double x0, double y0, double x1, double y1, int i) {
  //mbrot(X0, Y0, X1, Y1, I) when I < ?MAXITER, (X1*X1 + Y1*Y1) =< ?LIM_SQR ->
    //X2 = X1*X1 - Y1*Y1 + X0,
    //Y2 = 2*X1*Y1 + Y0,
    //mbrot(X0, Y0, X2, Y2, I + 1);
  //mbrot(_, _, _, _, I) -> I.
  //
  // from I < ?MAXITER, (X1*X1 + Y1*Y1) =< ?LIM_SQR
  // to 
  if (i < maxiter && (x1*x1 + y1*y1) <= lim_sqr) {
  } else {
    if (y1 == 1.0) {
      *avoid_opt += *avoid_opt;
    }
    return;
  }
  double x2 = x1 * x1 - y1 * y1 + x0;
  double y2 = 2 * x1 * y1 + y0;
  mbrot(x0, y0, x2, y2, i + 1); // tail recursive
}

void mbrot(double x, double y) {
  //mbrot(X,Y) -> mbrot(X,Y,X,Y,0).
  mbrot(x, y, x, y, 0);
}

void cols(size_t w, size_t h, size_t wi, size_t hi) {
  //cols(W, H, Wi, Hi) when Wi > 0 ->
    //%% transform X and Y pixel to mandelbrot coordinates
    //X = (Wi - 1)/W*(2*?RL) - ?RL,
    //Y = (Hi - 1)/H*(2*?IL) - ?IL,
    //%% do mandelbrot
    //mbrot(X, Y),
    //cols(W, H, Wi - 1, Hi);
  //cols(_, _, 0, _) -> ok.
  if (wi > 0) {
  } else {
    return;
  }
  double x = (wi -1)/w*(2*rl) -rl; 
  double y = (hi - 1)/h*(2*il) - il;
  mbrot(x, y);
  cols(w, h, wi -1, hi); // tail recursieve
}

inline void cols(size_t w, size_t h, size_t hi) {
  //cols(W, H, Hi) -> cols(W, H, W, Hi).
  cols(w,h,w,hi);
}

void rows(size_t w, size_t h, size_t hi) {
  //rows(W, H, Hi) when Hi > 0->
    //cols(W, H, Hi),
    //rows(W, H, Hi - 1);
  //rows(_, _, 0) -> ok.
  if (hi > 0) {
  } else {
    return;
  }
  cols(w, h, hi);
  rows(w, h, hi -1); // tail recursive
}

inline void rows(size_t w, size_t h) {
  //rows(W,H) -> rows(W, H, H).
  rows(w, h, h);
}

void worker(event_based_actor* self, size_t n, actor parent) {
  //worker(N, Parent) ->
    //rows(N, N),
    //Parent ! {self(), done}.
  rows(n,n);
  self->send(parent, done_atom::value);
}

void receive_workers(scoped_actor& self, erlang_pattern_matching<actor>&& pids) {
  //receive_workers([]) -> ok;
  //receive_workers([Pid|Pids]) -> receive {Pid, done} -> receive_workers(Pids) end.
  pids.restart();
  while(!pids.matched()) {
    self->receive([&](done_atom) {
      pids.match(actor_cast<actor>(self->current_sender()));
    });
  }
}

erlang_pattern_matching<actor> start_workers(scoped_actor& self, size_t n,
                                             size_t np) {
  //start_workers(N, Np) ->
    //start_workers(N, Np, self(), []).

  //start_workers(_, 0, _, Pids) -> Pids;
  //start_workers(N, Np, Me, Pids) ->
    //Pid = spawn_link(fun() -> worker(N, Me) end),
    //start_workers(N, Np - 1, Me, [Pid|Pids]).
  erlang_pattern_matching<actor> result;
  for (size_t i = 0; i < np; ++i) {
    result.match_list().emplace_back(
      self->spawn(worker, n, actor_cast<actor>(self)));
  }
  return result;
}

void run(actor_system& system, size_t n, size_t np) {
  scoped_actor self{system};
  receive_workers(self, start_workers(self, n, np));
}

void usage() {
  cout << "usage: bencherl_06_mbrot VERSION NUM_CORES" << endl
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
  //short -> [2, 4];
  //intermediate -> [4, 4];
  //long -> [7, 4]
  if (version == "test") {
    f = make_tuple(1,1);
  } else if (version == "short") {
    f = make_tuple(2, 4);
  } else if (version == "intermediate") {
    f = make_tuple(4, 4);
  } else if (version == "long") {
    f = make_tuple(7, 4);
  } else {
    std::cerr << "version musst be short, intermediate or long" << std::endl;
    exit(1);
  }
  avoid_opt = new int(1);
  int cores = std::stoi(argv[2]);
  actor_system_config cfg;
  cfg.parse(argc, argv, "caf-application.ini");
  actor_system system{cfg};
  auto n = get<1>(f) * cores;
  auto np = get<0>(f) * cores;
  run(system, n, np);
}


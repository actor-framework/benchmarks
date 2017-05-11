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
#include <vector>
#include <algorithm>
#include <map>

#include "caf/all.hpp"

using namespace std;
using std::chrono::seconds;
using namespace caf;

class config : public actor_system_config {
public:
  int n = 10000000; // num pieces
  int w = 100; // num workers
  double l = 1; // left end-point
  double r = 5; // right end-point
  static bool debug; // = false;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of pieces")
      .add(w, "www,w", " number of wokers")
      .add(l, "lll,l", "left end-point")
      .add(r, "rrr,r", "right eind-point")
      .add(debug, "ddd,d", "debug");
  }
};
bool config::debug = false;

double fx( double x) {
  double a = sin(pow(x, 3) - 1);
  double b = x + 1;
  double c = a / b;
  double d = sqrt(1 + exp(sqrt(2 * x)));
  double r = c * d;
  return r;
}

struct work_msg {
  double l;
  double r;
  double h;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(work_msg);

struct result_msg {
  double result;
  int worker_id;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(result_msg);

behavior worker_fun(event_based_actor* self, actor master, int id) {
  return {
    [=](work_msg& wm) {
      int n =  ((wm.r - wm.l) / wm.h);
      auto accum_area = 0.0;
      int i = 0;
      while (i < n) {
        auto lx = (i * wm.h) + wm.l;
        auto rx = lx + wm.h;
        auto ly = fx(lx);
        auto ry = fx(rx);
        auto area = 0.5 * (ly + ry) * wm.h;
        accum_area += area;
        i += 1;
      }
      self->send(master, result_msg{accum_area, id});
      self->quit();
    }
  };
}

struct master_state {
  vector<actor> workers;
  int num_terms_received;
  double result_area;
};

behavior master_fun(stateful_actor<master_state>* self, int num_workers) {
  auto& s = self->state;
  s.workers.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    s.workers.emplace_back(self->spawn(worker_fun, actor_cast<actor>(self), i));  
  }
  s.num_terms_received = 0;
  s.result_area = 0.0;
  return {
    [=](result_msg& rm) {
      auto& s = self->state; 
      ++s.num_terms_received;
      s.result_area += rm.result;
      if (s.num_terms_received == num_workers) {
        cout << "  Area: " << s.result_area << endl;
        self->quit();
      }
    },
    [=](work_msg& wm) {
      auto& s = self->state; 
      double worker_range = (wm.r - wm.l) / num_workers;
      int i = 0;
      for (auto& loop_worker : s.workers) {
        auto wl = (worker_range * i) + wm.l;
        auto wr = wl + worker_range;
        self->send(loop_worker, work_msg{wl, wr, wm.h});
        ++i; 
      }
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  auto num_workers = cfg.w;
  auto precision = (cfg.r -cfg.l) / cfg.n;
  auto master = system.spawn(master_fun, num_workers);
  anon_send(master, work_msg{cfg.l, cfg.r, precision});
}

CAF_MAIN()

/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include <vector>
#include <chrono>

#include "caf/all.hpp"

#include "caf/scheduler/profiled_coordinator.hpp"

using namespace std;
using namespace caf;
using namespace std::chrono;

using task_atom = atom_constant<atom("task")>;
using result_atom = atom_constant<atom("result")>;

template <class T>
auto ms(T x, T y) -> decltype(duration_cast<milliseconds>(y - x).count()) {
  return duration_cast<milliseconds>(y - x).count();
}

using hrc = high_resolution_clock;

using timestamp = decltype(hrc::now());

behavior task_worker(event_based_actor* self) {
  actor_ostream::redirect(self, "task_worker_" + std::to_string(self->id()));
  aout(self) << self->id() << " " << "task_worker" << endl;
  aout(self) << "hi there from " << self->id() << "! :)" << endl;
  return {
    [=](task_atom, int complexity, timestamp ts) -> int {
      //aout(self) << "delay until received: " << ms(ts, hrc::now()) << endl;
      int result = 0;
      auto x = uint64_t{1} << complexity;
      for (uint64_t j = 0; j < x; ++j) {
        for (int i = 0; i < 5000000; ++i) {
          ++result;
        }
      }
      return result;
    }
  };
}

behavior recursive_worker(event_based_actor* self, actor parent) {
  return {
    [=](task_atom, uint32_t x) {
      if (x == 1) {
        self->send(parent, result_atom::value, uint32_t{1});
        self->quit();
        return;
      }
      auto msg = make_message(task_atom::value, x - 1);
      self->send(self->spawn<lazy_init>(recursive_worker, self), msg);
      self->send(self->spawn<lazy_init>(recursive_worker, self), msg);
      self->become (
        [=](result_atom, uint32_t r1) {
          self->become (
            [=](result_atom, uint32_t r2) {
              if (parent != invalid_actor) {
                self->send(parent, result_atom::value, 1 + r1 + r2);
              }
              self->quit();
            }
          );
        }
      );
    }
  };
}

bool mandatory_missing(std::set<std::string> opts,
                       std::initializer_list<std::string> xs) {
  auto not_in_opts = [&](const std::string& x) {
    if (opts.count(x) == 0) {
      cerr << "mandatory argument missing: " << x << endl;
      return true;
    }
    return false;
  };
  return std::any_of(xs.begin(), xs.end(), not_in_opts);
}

bool setup(int argc, char** argv) {
  std::string profiler_output_file;
  std::string labels_output_file;
  long long profiler_resolution_ms = 100;
  size_t scheduler_threads = std::thread::hardware_concurrency();
  size_t max_msg_per_run = std::numeric_limits<size_t>::max();
  auto res = message_builder{argv + 1, argv + argc}.extract_opts({
    {"output,o", "output file for profiler (mandatory)", profiler_output_file},
    {"labels,l", "output file for labels (mandatory)", labels_output_file},
    {"resolution,r", "profiler resolution in ms", profiler_resolution_ms},
    {"threads,t", "number of threads for the scheduler", scheduler_threads},
    {"max-msgs,m", "number of messages per actor run", max_msg_per_run}
  });
  if (! res.error.empty() || res.opts.count("help") > 0
      || ! res.remainder.empty()
      || mandatory_missing(res.opts, {"output", "labels"})) {
    cout << res.error << endl << res.helptext << endl;
    return false;
  }
  using impl = scheduler::profiled_coordinator<>;
  set_scheduler(new impl{profiler_output_file,
                         milliseconds(profiler_resolution_ms),
                         scheduler_threads, max_msg_per_run});
  actor_ostream::redirect_all(labels_output_file);
  return true;
}

int main(int argc, char** argv) {
  if (! setup(argc, argv)) {
    return 1;
  }
  vector<actor> workers;
  for (int i = 0; i < 20; ++i) {
    workers.push_back(spawn<lazy_init>(task_worker));
  }
  for (int j = 0; j < 10; ++j) {
    for (int i = 0; i < 5; ++i) {
      for (auto& w : workers) {
        anon_send(w, task_atom::value, i, hrc::now());
      }
    }
  }
  for (auto& w : workers) {
    anon_send_exit(w, exit_reason::user_shutdown);
  }
  await_all_actors_done();
  shutdown();
}

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
  aout(self) << self->id() << " task_worker_" << self->id() << endl;
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

bool setup(int argc, char** argv, int& workload) {
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
    {"max-msgs,m", "number of messages per actor run", max_msg_per_run},
    {"workload,w", "select workload to bench (1-10) (mandatory)", workload}
  });
  if (! res.error.empty() || res.opts.count("help") > 0
      || ! res.remainder.empty()
      || mandatory_missing(res.opts, {"output", "labels", "workload"})) {
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

/// Spawn 20 `task_worker` and give them work,
/// the work has variation in its complexity (0 to 4)
void workload_1() {
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
}

/// Spawn 2^15 `recursive_worker`
void workload_2() {
  auto root = spawn(recursive_worker, invalid_actor);
  anon_send(root, task_atom::value, uint32_t{15});
}

/// Spawn 20 `task_worker`and give them work,
/// the work has variation in its complexity (0 to 4)
/// In addition, this will spawn 2^15 `recursive_worker`
void workload_3() {
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
  auto root = spawn(recursive_worker, invalid_actor);
  anon_send(root, task_atom::value, uint32_t{15});
  for (auto& w : workers) {
    anon_send_exit(w, exit_reason::user_shutdown);
  }
  anon_send_exit(root, exit_reason::user_shutdown);
}

/// Spawn 5 `task_worker` and give them work
/// then spawn 2^15 `recursive_worker`. This is
/// repeated 10 times.
void workload_4() {
  vector<actor> workers;
  for (int j = 0; j < 10; ++j) {
    for (int i = 0; i < 5; ++i) {
      for (auto& w : workers) {
        anon_send(w, task_atom::value, i, hrc::now());
      }
    }
    auto root = spawn(recursive_worker, invalid_actor);
    anon_send(root, task_atom::value, uint32_t{15});
    for (int i = 0; i < 5; ++i) {
      workers.push_back(spawn<lazy_init>(task_worker));
    }
    anon_send_exit(root, exit_reason::user_shutdown);
  }
  for (auto& w : workers) {
    anon_send_exit(w, exit_reason::user_shutdown);
  }
}

/// Spawn 5 `recursive_worker` in an actor pool
/// after that, 5 `task_worker` are spawnend in an actor pool
void workload_5() {
  auto factory = [] {
    return spawn(recursive_worker, invalid_actor);
  };
  auto pool = actor_pool::make(5, factory, actor_pool::broadcast());
  anon_send(pool, task_atom::value, uint32_t{15});
  auto factory_task = [] {
    return spawn(task_worker);
  };
  auto pool2 = actor_pool::make(5, factory_task, actor_pool::broadcast());
  for (int j = 0; j < 10; ++j) {
    for (int i = 0; i < 9; ++i) {
      anon_send(pool2, task_atom::value, i, hrc::now());
    }
  }
  anon_send_exit(pool2, exit_reason::user_shutdown);
}

/// Spawn either 2^15 `recursive_worker` or a actor pool with 10 actors
/// of `task_worker` type. This is repeated 20 times, on every even count this
/// workload will spawn `recursive_worker`, on odd count `task_worker`.
void workload_6() {
  for (int i = 0; i < 20; ++i) {
    if (i % 2) {
      auto root = spawn(recursive_worker, invalid_actor);
      anon_send(root, task_atom::value, uint32_t{15});
    } else {
      auto pool = actor_pool::make(10, []{ return spawn(task_worker); },
                                   actor_pool::broadcast());
      anon_send(pool, task_atom::value, i, hrc::now());
      anon_send_exit(pool, exit_reason::user_shutdown);
    }
  }
}

int main(int argc, char** argv) {
  int workload = 0;
  if (! setup(argc, argv, workload)) {
    return 1;
  }
  switch(workload) {
    case 1:
      workload_1();
      break;
    case 2:
      workload_2();
      break;
    case 3:
      workload_3();
      break;
    case 4:
      workload_4();
      break;
    case 5:
      workload_5();
      break;
    case 6:
      workload_6();
      break;
  }
  await_all_actors_done();
  shutdown();
}

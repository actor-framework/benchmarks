/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#if CAF_VERSION < 1800

using task_atom = caf::atom_constant<caf::atom("task")>;
using result_atom = caf::atom_constant<caf::atom("result")>;
constexpr task_atom task_atom_v = task_atom::value;
constexpr result_atom result_atom_v = result_atom::value;

#else

CAF_BEGIN_TYPE_ID_BLOCK(scheduling, first_custom_type_id)

  CAF_ADD_TYPE_ID(scheduling, (std::chrono::high_resolution_clock::time_point))

  CAF_ADD_ATOM(scheduling, task_atom)
  CAF_ADD_ATOM(scheduling, result_atom)

CAF_END_TYPE_ID_BLOCK(scheduling)

#endif

#if CAF_VERSION < 1700
using settings = caf::dictionary<caf::config_value::dictionary>;
constexpr caf::tick_atom tick_atom_v = caf::tick_atom::value;
#endif

using namespace caf;
using namespace std::chrono;

template <class T>
auto ms(T x, T y) -> decltype(duration_cast<milliseconds>(y - x).count()) {
  return duration_cast<milliseconds>(y - x).count();
}

using hrc = high_resolution_clock;

behavior task_worker(event_based_actor* self) {
  aout(self) << self->id() << " task_worker_" << self->id() << std::endl;
  return {
    [=](task_atom, int complexity, hrc::time_point) -> int {
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
        self->send(parent, result_atom_v, uint32_t{1});
        self->quit();
        return;
      }
      auto msg = make_message(task_atom_v, x - 1);
      self->send(self->spawn<lazy_init>(recursive_worker, self), msg);
      self->send(self->spawn<lazy_init>(recursive_worker, self), msg);
      self->become (
        [=](result_atom, uint32_t r1) {
          self->become (
            [=](result_atom, uint32_t r2) {
              self->send(parent, result_atom_v, 1 + r1 + r2);
              self->quit();
            }
          );
        }
      );
    }
  };
}

bool mandatory_missing(const settings& conf,
                       std::initializer_list<std::string> xs) {
  auto not_in_conf = [&](const std::string& x) {
    if (!conf.contains(x)) {
      std::cerr << "mandatory argument missing: " << x << std::endl;
      return true;
    }
    return false;
  };
  return std::any_of(xs.begin(), xs.end(), not_in_conf);
}

bool setup(int argc, char** argv, std::string& labels_output_file,
           int& workload, actor_system_config& cfg) {
  std::string profiler_output_file;
  size_t profiler_resolution_ms = 100;
  size_t scheduler_threads = std::thread::hardware_concurrency();
  size_t max_msg_per_run = std::numeric_limits<size_t>::max();
  config_option_set options;
  config_option_adder{options, "global"}
    .add<bool>("help,h?", "print this help text")
    .add(profiler_output_file, "output,o",
         "output file for profiler (mandatory)")
    .add(labels_output_file, "labels,l", "output file for labels (mandatory)")
    .add(profiler_resolution_ms, "resolution,r", "profiler resolution in ms")
    .add(scheduler_threads, "threads,t", "number of threads for the scheduler")
    .add(max_msg_per_run, "max-msgs,m", "number of messages per actor run")
    .add(workload, "workload,w", "select workload to bench (1-10) (mandatory)");
  settings conf;
  auto res = options.parse(conf, {argv + 1, argv + argc});
  if (res.first != caf::pec::success) {
    std::cerr << "error while parsing argument \"" << *res.second
              << "\": " << to_string(res.first) << "\n\n";
    std::cerr << options.help_text() << std::endl;
    return EXIT_FAILURE;
  }
  if (get_or(conf, "help", false) ||
      mandatory_missing(conf, {"output", "labels", "workload"})) {
    std::cout << options.help_text() << '\n';
    return false;
  }
  auto profiler_resolution = std::chrono::milliseconds{profiler_resolution_ms};
  cfg.set("scheduler.enable-profiling", true);
  cfg.set("scheduler.profiling-resolution", timespan{profiler_resolution});
  cfg.set("scheduler.max-threads", scheduler_threads);
  cfg.set("scheduler.max_throughput", max_msg_per_run);
  if (workload < 0 || workload > 5)
    return false;
  return true;
}

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(decltype(hrc::now()))

/// Spawn 20 `task_worker` and give them work,
/// the work has variation in its complexity (0 to 4)
void impl1(actor_system& system) {
  std::vector<actor> workers;
  for (int i = 0; i < 20; ++i)
    workers.push_back(system.spawn<lazy_init>(task_worker));
  for (int j = 0; j < 10; ++j)
    for (int i = 0; i < 5; ++i)
      for (auto& w : workers)
        anon_send(w, task_atom_v, i, hrc::now());
  for (auto& w : workers)
    anon_send_exit(w, exit_reason::user_shutdown);
}

/// Spawn 2^15 `recursive_worker`
void impl2(actor_system& system) {
  scoped_actor self{system};
  auto root = system.spawn(recursive_worker, self);
  anon_send(root, task_atom_v, uint32_t{15});
}

/// Spawn 20 `task_worker`and give them work,
/// the work has variation in its complexity (0 to 4)
/// In addition, this will spawn 2^15 `recursive_worker`
void impl3(actor_system& system) {
  scoped_actor self{system};
  std::vector<actor> workers;
  for (int i = 0; i < 20; ++i)
    workers.push_back(system.spawn<lazy_init>(task_worker));
  for (int j = 0; j < 10; ++j)
    for (int i = 0; i < 5; ++i)
      for (auto& w : workers)
        anon_send(w, task_atom_v, i, hrc::now());
  auto root = system.spawn(recursive_worker, self);
  anon_send(root, task_atom_v, uint32_t{15});
  for (auto& w : workers)
    anon_send_exit(w, exit_reason::user_shutdown);
  anon_send_exit(root, exit_reason::user_shutdown);
}

/// Spawn 5 `task_worker` and give them work
/// then spawn 2^15 `recursive_worker`. This is
/// repeated 10 times.
void impl4(actor_system& system) {
  scoped_actor self{system};
  std::vector<actor> workers;
  for (int j = 0; j < 10; ++j) {
    for (int i = 0; i < 5; ++i)
      for (auto& w : workers)
        anon_send(w, task_atom_v, i, hrc::now());
    auto root = system.spawn(recursive_worker, self);
    anon_send(root, task_atom_v, uint32_t{15});
    for (int i = 0; i < 5; ++i)
      workers.push_back(system.spawn<lazy_init>(task_worker));
    anon_send_exit(root, exit_reason::user_shutdown);
  }
  for (auto& w : workers)
    anon_send_exit(w, exit_reason::user_shutdown);
}

/// Spawn 5 `recursive_worker` in an actor pool
/// after that, 5 `task_worker` are spawnend in an actor pool
void impl5(actor_system& system) {
  scoped_actor self{system};
  auto factory = [&] {
    return system.spawn(recursive_worker, self);
  };
  auto pool = actor_pool::make(system.dummy_execution_unit(),
                               5, factory, actor_pool::broadcast());
  anon_send(pool, task_atom_v, uint32_t{15});
  auto factory_task = [&] {
    return system.spawn(task_worker);
  };
  auto pool2 = actor_pool::make(system.dummy_execution_unit(),
                                5, factory_task, actor_pool::broadcast());
  for (int j = 0; j < 10; ++j) {
    for (int i = 0; i < 9; ++i) {
      anon_send(pool2, task_atom_v, i, hrc::now());
    }
  }
  anon_send_exit(pool2, exit_reason::user_shutdown);
}

/// Spawn either 2^15 `recursive_worker` or a actor pool with 10 actors
/// of `task_worker` type. This is repeated 20 times, on every even count this
/// workload will spawn `recursive_worker`, on odd count `task_worker`.
void impl6(actor_system& system) {
  scoped_actor self{system};
  for (int i = 0; i < 20; ++i) {
    if (i % 2) {
      anon_send(system.spawn(recursive_worker, self),
                task_atom_v, uint32_t{15});
    } else {
      auto pool = actor_pool::make(system.dummy_execution_unit(), 10,
                                   [&]{ return system.spawn(task_worker); },
                                   actor_pool::broadcast());
      anon_send(pool, task_atom_v, i, hrc::now());
      anon_send_exit(pool, exit_reason::user_shutdown);
    }
  }
}

int main(int argc, char** argv) {
#if CAF_VERSION >= 1800
  caf::init_global_meta_objects<caf::id_block::scheduling>();
  caf::core::init_global_meta_objects();
#endif
  int workload = 0;
  actor_system_config cfg;
  std::string labels_output_file;
  if (!setup(argc, argv, labels_output_file, workload, cfg))
    return 1;
  actor_system system(cfg);
  actor_ostream::redirect_all(system, labels_output_file);
  using implfun = void (*)(actor_system&);
  implfun funs[] = {impl1, impl2, impl3, impl4, impl5, impl6};
  funs[workload](system);
}

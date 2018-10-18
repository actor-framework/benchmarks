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

// this file contains some micro benchmarks
// for various CAF implementation details

#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

using namespace caf;

namespace {

constexpr size_t num_messages = 1000000;

size_t s_invoked = 0;

// returns nr. of ms
void message_creation_native(benchmark::State& state) {
  for (auto _ : state) {
    auto msg = make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

void message_creation_dynamic(benchmark::State& state) {
  for (auto _ : state) {
    message_builder mb;
    message msg = mb.append(size_t{0}).to_message();
    benchmark::DoNotOptimize(msg);
  }
}

struct foo {
  int a;
  int b;
};

inline bool operator==(const foo& lhs, const foo& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

struct bar {
  foo a;
  std::string b;
};

inline bool operator==(const bar& lhs, const bar& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

} // namespace <anonymous>

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(foo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bar)

namespace {

template <class... Ts>
message make_dynamic_message(Ts&&... xs) {
  message_builder mb;
  return mb.append_all(std::forward<Ts>(xs)...).to_message();
}

struct SingleSystem : benchmark::Fixture {
  actor_system_config cfg;
  actor_system sys{cfg};

  message native_two_ints = make_message(1, 2);
  message native_two_doubles = make_message(1.0, 2.0);
  message native_two_strings = make_message("hi", "there");
  message native_one_foo = make_message(foo{1, 2});
  message native_one_bar = make_message(bar{foo{1, 2}});

  message dynamic_two_ints = make_dynamic_message(1, 2);
  message dynamic_two_doubles = make_dynamic_message(1.0, 2.0);
  message dynamic_two_strings = make_dynamic_message("hi", "there");
  message dynamic_one_foo = make_dynamic_message(foo{1, 2});
  message dynamic_one_bar = make_dynamic_message(bar{foo{1, 2}});

  behavior bhvr = behavior{
    [&](int) {
      s_invoked = 1;
    },
    [&](int, int) {
      s_invoked = 2;
    },
    [&](double) {
      s_invoked = 3;
    },
    [&](double, double) {
      s_invoked = 4;
    },
    [&](const std::string&) {
      s_invoked = 5;
    },
    [&](const std::string&, const std::string&) {
      s_invoked = 6;
    },
    [&](const foo&) {
      s_invoked = 7;
    },
    [&](const bar&) {
      s_invoked = 8;
    }
  };

  bool match(benchmark::State &state, message &msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    bhvr(msg);
    if (s_invoked != expected_handler_id) {
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }
};

BENCHMARK_DEFINE_F(SingleSystem, MatchNative)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7)
        || !match(state, native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(SingleSystem, MatchNative);

BENCHMARK_DEFINE_F(SingleSystem, MatchDynamic)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, dynamic_two_ints, 2)
        || !match(state, dynamic_two_doubles, 4)
        || !match(state, dynamic_two_strings, 6)
        || !match(state, dynamic_one_foo, 7)
        || !match(state, dynamic_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(SingleSystem, MatchDynamic);

struct source_state {
  const char* name = "source";
};

void source(stateful_actor<source_state> *self, actor dest,
            size_t max_messages) {
  self->make_source(
    dest,
    // initialize state
    [](size_t& n) {
      n = 0;
    },
    // get next element
    [=](size_t& n, downstream<uint64_t>& out, size_t hint) {
      auto num = std::min(hint, max_messages - n);
      for (size_t i = 0; i < num; ++i)
        out.push(i);
      n += num;
    },
    // check whether we reached the end
    [=](const size_t& n) {
      return n == max_messages;
    }
  );
}

struct stage_state {
  const char* name = "stage";
};

behavior stage(stateful_actor<stage_state>* self) {
  return {
    [=](const stream<uint64_t>& in) {
      return self->make_stage(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) {
          xs.push(x); },
        // cleanup
        [=](unit_t&) {
          // nop
        }
      );
    }
  };
}

struct fork_state {
  const char* name = "fork";
};

behavior fork(stateful_actor<fork_state>* self, vector<actor> sinks) {
  auto mgr = self->make_continuous_stage(
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) {
      xs.push(x);
    },
    // cleanup
    [=](unit_t&) {
      // nop
    }
  );
  for (auto& snk : sinks)
    mgr->add_outbound_path(snk);
  return {
    [=](stream<uint64_t> in) {
      self->unbecome();
      mgr->continuous(false);
      return mgr->add_inbound_path(in);
    }
  };
}

struct sink_state {
  const char* name = "sink";
};

behavior sink(stateful_actor<sink_state>* self, actor done_listener) {
  return {
    [=](stream<uint64_t> in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](size_t& count) {
          count = 0;
        },
        // processing step
        [=](size_t& count, uint64_t) {
          if (++count == num_messages) {
            self->send(done_listener, ok_atom::value);
            count = 0;
          }
        },
        // cleanup
        [=](size_t&) {
          // nop
        }
      );
    }
  };
}

BENCHMARK_DEFINE_F(SingleSystem, StreamPipeline)(benchmark::State& state) {
  for (auto _ : state) {
    {
      auto snk = sys.spawn(sink, actor{});
      for (auto i = 0; i < state.range(0); ++i)
        snk = snk * sys.spawn(stage);
      sys.spawn(source, snk, num_messages);
    }
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(SingleSystem, StreamPipeline)
    ->Arg(0)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Arg(4);

BENCHMARK_DEFINE_F(SingleSystem, StreamFork)(benchmark::State& state) {
  for (auto _ : state) {
    {
      vector<actor> sinks;
      for (auto i = 0; i < state.range(0); ++i)
        sinks.emplace_back(sys.spawn(sink, actor{}));
      sys.spawn(source, sys.spawn(fork, std::move(sinks)), num_messages);
    }
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(SingleSystem, StreamFork)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Arg(4);

BENCHMARK_DEFINE_F(SingleSystem, AsyncPipeline)(benchmark::State& state) {
  auto sender = [](event_based_actor* self, actor snk) {
    for (uint64_t i = 0; i < num_messages; ++i)
      self->send(snk, i);
  };
  auto relay = [](event_based_actor* self, actor snk) -> behavior {
    return {
      [=](uint64_t x) {
        self->send(snk, x);
      }
    };
  };
  auto receiver = []() -> behavior{
    return {
      [](uint64_t) {
        // nop
      }
    };
  };
  for (auto _ : state) {
    {
      auto snk = sys.spawn(receiver);
      for (auto i = 0; i < state.range(0); ++i)
        snk = sys.spawn(relay, snk);
      sys.spawn(sender, snk);
    }
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(SingleSystem, AsyncPipeline)
    ->Arg(0)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Arg(4);

struct continuous_stage_state {
  const char* name = "continuous_stage";
};

behavior continuous_stage(stateful_actor<continuous_stage_state> *self,
                          actor next_hop) {
  auto mgr = self->make_continuous_stage(
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) {
      xs.push(x); },
    // cleanup
    [=](unit_t&) {
      // nop
    }
  );
  mgr->add_outbound_path(next_hop);
  return {
    [=](const stream<uint64_t>& in) {
      mgr->add_inbound_path(in);
    }
  };
}

template <size_t NumStages>
struct ManySystems : benchmark::Fixture {
  struct config : actor_system_config {
    config() {
      load<io::middleman>();
      add_message_type_impl<stream<uint64_t>>("stream<uint64_t>");
      add_message_type_impl<vector<uint64_t>>("vector<uint64_t>");
    }
  };

  struct node {
    config cfg;
    actor_system sys{cfg};
    actor hdl;
    uint16_t port;
  };

  actor first_hop;

  static constexpr size_t num_nodes = NumStages + 2;

  static constexpr size_t source_node_id = 0;

  static constexpr size_t sink_node_id = NumStages + 1;

  std::array<node, num_nodes> nodes;

  scoped_actor sink_listener;

  template <class T>
  T unbox(expected<T> x) {
    if (!x)
      throw std::runtime_error("unbox failed");
    return std::move(*x);
  }

  ManySystems() : sink_listener{nodes.back().sys} {
    uint16_t first_hop_port = 0;
    auto& sink_node = nodes.back();
    sink_node.hdl = sink_node.sys.spawn(sink, sink_listener);
    sink_node.port = unbox(sink_node.sys.middleman().publish(sink_node.hdl, 0u));
    for (size_t i = NumStages; i > 0; --i) {
      auto& x = nodes[i];
      auto next_hop = unbox(x.sys.middleman().remote_actor("127.0.0.1",
                                                           nodes[i + 1].port));
      x.hdl = x.sys.spawn(continuous_stage, next_hop);
      x.port = unbox(x.sys.middleman().publish(x.hdl, 0u));
    }
    first_hop_port = nodes[1].port;
    first_hop = unbox(nodes[0].sys.middleman().remote_actor("127.0.0.1",
                                                            first_hop_port));
  }

  ~ManySystems() {
    for (auto& x : nodes)
      anon_send_exit(x.hdl, exit_reason::user_shutdown);
  }

  void run() {
    nodes.front().sys.spawn(source, first_hop, num_messages);
    sink_listener->receive(
      [](ok_atom) {
        // nop
      }
    );
  }
};

#define ManySystemsStreamPipeline(num)                                         \
  BENCHMARK_TEMPLATE_F(ManySystems, StreamPipeline_##num, num)                 \
  (benchmark::State & state) {                                                 \
    for (auto _ : state)                                                       \
      run();                                                                   \
  }

ManySystemsStreamPipeline(0)
ManySystemsStreamPipeline(1)
ManySystemsStreamPipeline(2)
ManySystemsStreamPipeline(3)
ManySystemsStreamPipeline(4)

} // namespace <anonymous>

BENCHMARK_MAIN();

#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#ifdef CAF_BEGIN_TYPE_ID_BLOCK

CAF_BEGIN_TYPE_ID_BLOCK(streaming, first_custom_type_id)

  CAF_ADD_TYPE_ID(streaming, (caf::stream<uint64_t>) );
  CAF_ADD_TYPE_ID(streaming, (std::vector<uint64_t>) );

CAF_END_TYPE_ID_BLOCK(streaming)

#endif

using std::vector;

using namespace caf;

// -- constants and global state -----------------------------------------------

namespace {

constexpr size_t num_messages = 1'000'000;

} // namespace <anonymous>

// -- utility for running streaming benchmarks ---------------------------------

void StreamingSettings(benchmark::internal::Benchmark* b) {
  for (int i = 0; i <= 4; ++i)
    for (int j = 1; j <= 1'000'000; j *= 10)
      b->Args({i, j});
}

// -- simple integer source ----------------------------------------------------

struct source_state {
  const char* name = "source";
};

void source(stateful_actor<source_state> *self, actor dest,
            size_t max_messages) {
  attach_stream_source(
    self, dest,
    // initialize state
    [](size_t& n) { n = 0; },
    // get next element
    [=](size_t& n, downstream<uint64_t>& out, size_t hint) {
      auto num = std::min(hint, max_messages - n);
      for (size_t i = 0; i < num; ++i)
        out.push(i);
      n += num;
    },
    // check whether we reached the end
    [=](const size_t& n) { return n == max_messages; });
}

// -- simple integer stage -----------------------------------------------------

struct stage_state {
  const char* name = "stage";
};

behavior stage(stateful_actor<stage_state>* self) {
  return {
    [=](stream<uint64_t> in) {
      return attach_stream_stage(
        self,
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) { xs.push(x); },
        // cleanup
        [=](unit_t&) {
          // nop
        });
    },
  };
}

// -- simple integer fork state ------------------------------------------------

struct fork_state {
  const char* name = "fork";
};

behavior fork(stateful_actor<fork_state>* self, vector<actor> sinks) {
  auto mgr = attach_continuous_stream_stage(
    self,
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) { xs.push(x); },
    // cleanup
    [=](unit_t&) {
      // nop
    });
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

// -- simple stage for building pipelines one-by-one ---------------------------

struct continuous_stage_state {
  const char* name = "continuous_stage";
};

behavior continuous_stage(stateful_actor<continuous_stage_state> *self,
                          actor next_hop) {
  auto mgr = attach_continuous_stream_stage(
    self,
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [=](unit_t&, downstream<uint64_t>& xs, uint64_t x) { xs.push(x); },
    // cleanup
    [=](unit_t&) {
      // nop
    });
  mgr->add_outbound_path(next_hop);
  return {
    [=](const stream<uint64_t>& in) {
      mgr->add_inbound_path(in);
    }
  };
}

// -- simple integer sink ------------------------------------------------------

struct sink_state {
  const char* name = "sink";
};

behavior sink(stateful_actor<sink_state>* self, actor done_listener) {
  return {
    [=](stream<uint64_t> in) {
      return attach_stream_sink(
        self,
        // input stream
        in,
        // initialize state
        [](size_t& count) { count = 0; },
        // processing step
        [=](size_t& count, uint64_t) {
          if (++count == num_messages) {
            self->send(done_listener, ok_atom_v);
            count = 0;
          }
        },
        // cleanup
        [=](size_t&) {
          // nop
        });
    },
  };
}

// -- fixture for single-system streaming --------------------------------------

struct SingleSystem : benchmark::Fixture {
  actor_system_config cfg;
  actor_system sys{cfg};
};

BENCHMARK_DEFINE_F(SingleSystem, StreamPipeline)(benchmark::State& state) {
  for (auto _ : state) {
    {
      auto snk = sys.spawn(sink, actor{});
      for (auto i = 0; i < state.range(0); ++i)
        snk = snk * sys.spawn(stage);
      sys.spawn(source, snk, static_cast<size_t>(state.range(1)));
    }
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(SingleSystem, StreamPipeline)
    ->Apply(StreamingSettings);

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
    ->Apply(StreamingSettings);

BENCHMARK_DEFINE_F(SingleSystem, MessagePipeline)(benchmark::State& state) {
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

BENCHMARK_REGISTER_F(SingleSystem, MessagePipeline)
    ->Apply(StreamingSettings);

// -- fixture for multi-system streaming ---------------------------------------

template <size_t NumStages>
struct ManySystems : benchmark::Fixture {
  struct config : actor_system_config {
    config() {
      load<io::middleman>();
#ifdef CAF_BEGIN_TYPE_ID_BLOCK
      init_global_meta_objects<caf::id_block::streaming>();
#else
      add_message_type_impl<stream<uint64_t>>("stream<uint64_t>");
      add_message_type_impl<vector<uint64_t>>("vector<uint64_t>");
#endif
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

BENCHMARK_MAIN();

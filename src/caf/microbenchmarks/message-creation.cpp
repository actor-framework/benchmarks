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

struct MessageCreation : benchmark::Fixture {
  MessageCreation() {
#if CAF_VERSION >= 1800
    caf::core::init_global_meta_objects();
#endif
  }
};

// -- benchmarking of message creation -----------------------------------------

BENCHMARK_DEFINE_F(MessageCreation, NativeCreation)(benchmark::State& state) {
  for (auto _ : state) {
    auto msg = make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(MessageCreation, NativeCreation);

BENCHMARK_DEFINE_F(MessageCreation, DynamicCreation)(benchmark::State& state) {
  for (auto _ : state) {
    message_builder mb;
    message msg = mb.append(size_t{0}).to_message();
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(MessageCreation, DynamicCreation);

BENCHMARK_MAIN();

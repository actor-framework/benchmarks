#include <chrono>
#include <cstdint>
#include <iostream>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

template <class... Ts>
config_value cfg_lst(Ts&&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

struct Messages : benchmark::Fixture {
  /// A message featuring a recursive data type (config_value).
  message recursive;

  /// The serialized representation of `recursive` from the binary serializer.
  binary_serializer::container_type binary_serialized;

  Messages() {
#if CAF_VERSION >= 1800
    caf::core::init_global_meta_objects();
#endif
    config_value::dictionary dict;
    put(dict, "scheduler.policy", "none");
    put(dict, "scheduler.max-threads", 42);
    put(dict, "nodes.preload",
        cfg_lst("sun", "venus", "mercury", "earth", "mars"));
    recursive = make_message(config_value{std::move(dict)});
    binary_serializer s1{nullptr, binary_serialized};
    auto res = inspect(s1, recursive);
    static_cast<void>(res); // Discard.
  }
};

BENCHMARK_DEFINE_F(Messages, BinarySerializer)(benchmark::State& state) {
  for (auto _ : state) {
    binary_serializer::container_type buf;
    buf.reserve(512);
    binary_serializer sink{nullptr, buf};
    auto res = inspect(sink, recursive);
    static_cast<void>(res); // Discard.
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, BinarySerializer);

BENCHMARK_DEFINE_F(Messages, BinaryDeserializer)(benchmark::State& state) {
  for (auto _ : state) {
    message result;
    binary_deserializer source{nullptr, binary_serialized};
    auto res = inspect(source, result);
    static_cast<void>(res); // Discard.
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_REGISTER_F(Messages, BinaryDeserializer);

BENCHMARK_MAIN();

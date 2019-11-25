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

#include <chrono>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "v2.hpp"
#include "v3.hpp"
#include "v4.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

using namespace caf;

// -- constants and global state -----------------------------------------------

namespace {

constexpr size_t num_messages = 1000000;

size_t s_invoked = 0;

} // namespace

// -- benchmarking of message creation -----------------------------------------

void NativeMessageCreation(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreation);

void NativeMessageCreationV2(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = v2::make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreationV2);

void NativeMessageCreationV3(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = v3::make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreationV3);

void NativeMessageCreationV4(benchmark::State &state) {
  for (auto _ : state) {
    auto msg = v4::make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK(NativeMessageCreationV4);

// void DynamicMessageCreation(benchmark::State &state) {
//   for (auto _ : state) {
//     message_builder mb;
//     message msg = mb.append(size_t{0}).to_message();
//     benchmark::DoNotOptimize(msg);
//   }
// }
//
// BENCHMARK(DynamicMessageCreation);

// -- custom message type ------------------------------------------------------

struct foo {
  int a;
  int b;
};

inline bool operator==(const foo &lhs, const foo &rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

struct bar {
  foo a;
  std::string b;
};

inline bool operator==(const bar &lhs, const bar &rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(foo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bar)

ADD_V2_MSG_TYPE(100, int)
ADD_V2_MSG_TYPE(101, int, int)
ADD_V2_MSG_TYPE(102, double)
ADD_V2_MSG_TYPE(103, double, double)
ADD_V2_MSG_TYPE(104, std::string)
ADD_V2_MSG_TYPE(105, std::string, std::string)
ADD_V2_MSG_TYPE(106, foo)
ADD_V2_MSG_TYPE(107, bar)

static size_t v2_dummy = v2::fill_type_registry({
  v2::make_serialization_info<int>(100),
  v2::make_serialization_info<int, int>(101),
  v2::make_serialization_info<double>(102),
  v2::make_serialization_info<double, double>(103),
  v2::make_serialization_info<std::string>(104),
  v2::make_serialization_info<std::string, std::string>(105),
});

ADD_V3_TYPE(100, size_t)
ADD_V3_TYPE(101, int)
ADD_V3_TYPE(102, double)
ADD_V3_TYPE(103, std::string)
ADD_V3_TYPE(104, foo)
ADD_V3_TYPE(105, bar)

static size_t v3_dummy = v3::fill_type_registry({
  v3::make_serialization_info<int>(),
  v3::make_serialization_info<int, int>(),
  v3::make_serialization_info<double>(),
  v3::make_serialization_info<double, double>(),
  v3::make_serialization_info<std::string>(),
  v3::make_serialization_info<std::string, std::string>(),
});

CAF_BEGIN_MSG_TYPES(bench)

CAF_ADD_MSG_TYPE(bench, size_t)
CAF_ADD_MSG_TYPE(bench, int)
CAF_ADD_MSG_TYPE(bench, double)
CAF_ADD_MSG_TYPE(bench, std::string)
CAF_ADD_MSG_TYPE(bench, foo)
CAF_ADD_MSG_TYPE(bench, bar)

CAF_END_MSG_TYPES(bench)


// -- pattern matching benchmark -----------------------------------------------

template <class... Ts> message make_dynamic_message(Ts &&... xs) {
  message_builder mb;
  return mb.append_all(std::forward<Ts>(xs)...).to_message();
}

template <class... Ts> config_value cfg_lst(Ts &&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

using namespace std::string_literals;

struct Messages : benchmark::Fixture {

  message native_two_ints = make_message(1, 2);
  message native_two_doubles = make_message(1.0, 2.0);
  message native_two_strings = make_message("hi", "there");
  message native_one_foo = make_message(foo{1, 2});
  message native_one_bar = make_message(bar{foo{1, 2}});

  v2::message v2_native_two_ints = v2::make_message(1, 2);
  v2::message v2_native_two_doubles = v2::make_message(1.0, 2.0);
  v2::message v2_native_two_strings = v2::make_message("hi"s, "there"s);
  v2::message v2_native_one_foo = v2::make_message(foo{1, 2});
  v2::message v2_native_one_bar = v2::make_message(bar{foo{1, 2}});

  v3::message v3_native_two_ints = v3::make_message(1, 2);
  v3::message v3_native_two_doubles = v3::make_message(1.0, 2.0);
  v3::message v3_native_two_strings = v3::make_message("hi"s, "there"s);
  v3::message v3_native_one_foo = v3::make_message(foo{1, 2});
  v3::message v3_native_one_bar = v3::make_message(bar{foo{1, 2}});

  v4::message v4_native_two_ints = v4::make_message(1, 2);
  v4::message v4_native_two_doubles = v4::make_message(1.0, 2.0);
  v4::message v4_native_two_strings = v4::make_message("hi"s, "there"s);
  v4::message v4_native_one_foo = v4::make_message(foo{1, 2});
  v4::message v4_native_one_bar = v4::make_message(bar{foo{1, 2}});

  message dynamic_two_ints = make_dynamic_message(1, 2);
  message dynamic_two_doubles = make_dynamic_message(1.0, 2.0);
  message dynamic_two_strings = make_dynamic_message("hi", "there");
  message dynamic_one_foo = make_dynamic_message(foo{1, 2});
  message dynamic_one_bar = make_dynamic_message(bar{foo{1, 2}});

  /// A message featuring a recursive data type (config_value).
  message recursive;

  std::vector<char> native_two_strings_serialized;

  std::vector<char> v2_native_two_strings_serialized;

  std::vector<char> v3_native_two_strings_serialized;

  std::vector<char> v4_native_two_strings_serialized;

  /// The serialized representation of `recursive` from the binary serializer.
  std::vector<char> recursive_binary_serialized;

  /// The serialized representation of `recursive` from the stream serializer.
  std::vector<char> recursive_stream_serialized;

  actor_system_config cfg;

  actor_system sys;

  Messages() : sys(cfg.set("scheduler.policy", atom("testing"))) {
    bench_announce_message_types();
    // config_value::dictionary dict;
    // put(dict, "scheduler.policy", atom("none"));
    // put(dict, "scheduler.max-threads", 42);
    // put(dict, "nodes.preload",
    //     cfg_lst("sun", "venus", "mercury", "earth", "mars"));
    // recursive = make_message(config_value{std::move(dict)});
    {
      stream_serializer<vectorbuf> sink{sys, native_two_strings_serialized};
      native_two_strings.save(sink);
    }
    {
      stream_serializer<vectorbuf> sink{sys, v2_native_two_strings_serialized};
      v2_native_two_strings.save(sink);
    }
    {
      stream_serializer<vectorbuf> sink{sys, v3_native_two_strings_serialized};
      v3_native_two_strings.save(sink);
    }
    {
      stream_serializer<vectorbuf> sink{sys, v4_native_two_strings_serialized};
      v4_native_two_strings.save(sink);
    }
    {
      binary_serializer sink{sys, recursive_binary_serialized};
      inspect(sink, recursive);
    }
    {
      stream_serializer<vectorbuf> sink{sys, recursive_stream_serialized};
      inspect(sink, recursive);
    }
  }

  behavior bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  v2::behavior v2_bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  v3::behavior v3_bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  v4::behavior v4_bhvr{
    [&](int) { s_invoked = 1; },
    [&](int, int) { s_invoked = 2; },
    [&](double) { s_invoked = 3; },
    [&](double, double) { s_invoked = 4; },
    [&](const std::string&) { s_invoked = 5; },
    [&](const std::string&, const std::string&) { s_invoked = 6; },
    [&](const foo&) { s_invoked = 7; },
    [&](const bar&) { s_invoked = 8; },
  };

  bool match(benchmark::State& state, message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    bhvr(msg);
    if (s_invoked != expected_handler_id) {
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }

  bool match(benchmark::State& state, v2::message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    v2_bhvr(msg);
    if (s_invoked != expected_handler_id) {
      printf("UH OH: invoked %d instead of %d!\n", (int) s_invoked,
             (int) expected_handler_id);
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }

  bool match(benchmark::State& state, v3::message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    v3_bhvr(msg);
    if (s_invoked != expected_handler_id) {
      printf("UH OH: invoked %d instead of %d!\n", (int) s_invoked,
             (int) expected_handler_id);
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }

  bool match(benchmark::State& state, v4::message& msg,
             size_t expected_handler_id) {
    s_invoked = 0;
    v4_bhvr(msg);
    if (s_invoked != expected_handler_id) {
      printf("UH OH: invoked %d instead of %d!\n", (int) s_invoked,
             (int) expected_handler_id);
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }
};

BENCHMARK_DEFINE_F(Messages, MatchNative)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7) || !match(state, native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNative);

BENCHMARK_DEFINE_F(Messages, MatchNativeV2)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, v2_native_two_ints, 2)
        || !match(state, v2_native_two_doubles, 4)
        || !match(state, v2_native_two_strings, 6)
        || !match(state, v2_native_one_foo, 7)
        || !match(state, v2_native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNativeV2);

BENCHMARK_DEFINE_F(Messages, MatchNativeV3)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, v3_native_two_ints, 2)
        || !match(state, v3_native_two_doubles, 4)
        || !match(state, v3_native_two_strings, 6)
        || !match(state, v3_native_one_foo, 7)
        || !match(state, v3_native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNativeV3);

BENCHMARK_DEFINE_F(Messages, MatchNativeV4)(benchmark::State &state) {
  for (auto _ : state) {
    if (!match(state, v4_native_two_ints, 2)
        || !match(state, v4_native_two_doubles, 4)
        || !match(state, v4_native_two_strings, 6)
        || !match(state, v4_native_one_foo, 7)
        || !match(state, v4_native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNativeV4);

// BENCHMARK_DEFINE_F(Messages, MatchDynamic)(benchmark::State &state) {
//   for (auto _ : state) {
//     if (!match(state, dynamic_two_ints, 2)
//         || !match(state, dynamic_two_doubles, 4)
//         || !match(state, dynamic_two_strings, 6)
//         || !match(state, dynamic_one_foo, 7)
//         || !match(state, dynamic_one_bar, 8))
//       break;
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, MatchDynamic);

// -- serialization of simple string messages ----------------------------------

BENCHMARK_DEFINE_F(Messages, SerializeStringMessage)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessage);

BENCHMARK_DEFINE_F(Messages, SerializeStringMessageV2)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    v2_native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessageV2);

BENCHMARK_DEFINE_F(Messages, SerializeStringMessageV3)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    v3_native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessageV3);

BENCHMARK_DEFINE_F(Messages, SerializeStringMessageV4)(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<char> buf;
    buf.reserve(512);
    binary_serializer bs{sys, buf};
    v4_native_two_strings.save(bs);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_REGISTER_F(Messages, SerializeStringMessageV4);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessage)(benchmark::State &state) {
  for (auto _ : state) {
    message msg;
    binary_deserializer bs{sys, native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessage);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessageV2)(benchmark::State &state) {
  for (auto _ : state) {
    v2::message msg;
    binary_deserializer bs{sys, v2_native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessageV2);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessageV3)(benchmark::State &state) {
  for (auto _ : state) {
    v3::message msg;
    binary_deserializer bs{sys, v3_native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessageV3);

BENCHMARK_DEFINE_F(Messages, DeserializeStringMessageV4)(benchmark::State &state) {
  for (auto _ : state) {
    v4::message msg;
    binary_deserializer bs{sys, v4_native_two_strings_serialized};
    msg.load(bs);
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_REGISTER_F(Messages, DeserializeStringMessageV4);

// -- serialization of recursive data ------------------------------------------

// BENCHMARK_DEFINE_F(Messages, BinarySerializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     std::vector<char> buf;
//     buf.reserve(512);
//     binary_serializer bs{sys, buf};
//     inspect(bs, recursive);
//     benchmark::DoNotOptimize(buf);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, BinarySerializer);
//
// BENCHMARK_DEFINE_F(Messages, StreamSerializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     std::vector<char> buf;
//     buf.reserve(512);
//     stream_serializer<vectorbuf> bs{sys, buf};
//     inspect(bs, recursive);
//     benchmark::DoNotOptimize(buf);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, StreamSerializer);
//
// BENCHMARK_DEFINE_F(Messages, BinaryDeserializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     message result;
//     binary_deserializer source{sys, recursive_binary_serialized};
//     inspect(source, result);
//     benchmark::DoNotOptimize(result);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, BinaryDeserializer);
//
// BENCHMARK_DEFINE_F(Messages, StreamDeserializer)(benchmark::State &state) {
//   for (auto _ : state) {
//     message result;
//     stream_deserializer<charbuf> source{sys, recursive_stream_serialized};
//     inspect(source, result);
//     benchmark::DoNotOptimize(result);
//   }
// }
//
// BENCHMARK_REGISTER_F(Messages, StreamDeserializer);

BENCHMARK_MAIN();

#include <cstdint>

#include <benchmark/benchmark.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

struct bar;
struct foo;

#ifdef CAF_BEGIN_TYPE_ID_BLOCK

CAF_BEGIN_TYPE_ID_BLOCK(pattern_matching, first_custom_type_id)

  CAF_ADD_TYPE_ID(pattern_matching, (bar));
  CAF_ADD_TYPE_ID(pattern_matching, (foo));

CAF_END_TYPE_ID_BLOCK(pattern_matching)

caf::string_view atom(caf::string_view str) {
  return str;
}

#endif

using namespace caf;

// -- constants and global state -----------------------------------------------

namespace {

size_t s_invoked = 0;

} // namespace

// -- custom message type ------------------------------------------------------

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

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(foo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bar)

// -- pattern matching benchmark -----------------------------------------------

template <class... Ts>
message make_dynamic_message(Ts&&... xs) {
  message_builder mb;
  return mb.append_all(std::forward<Ts>(xs)...).to_message();
}

template <class... Ts>
config_value cfg_lst(Ts&&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

struct Messages : benchmark::Fixture {
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

  Messages() {
#if CAF_VERSION >= 1800
    caf::init_global_meta_objects<caf::id_block::pattern_matching>();
    caf::core::init_global_meta_objects();
#endif
  }

  behavior bhvr = behavior{
    [](int) { s_invoked = 1; },
    [](int, int) { s_invoked = 2; },
    [](double) { s_invoked = 3; },
    [](double, double) { s_invoked = 4; },
    [](const std::string&) { s_invoked = 5; },
    [](const std::string&, const std::string&) { s_invoked = 6; },
    [](const foo&) { s_invoked = 7; },
    [](const bar&) { s_invoked = 8; },
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
};

BENCHMARK_DEFINE_F(Messages, MatchNative)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7) || !match(state, native_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchNative);

BENCHMARK_DEFINE_F(Messages, MatchDynamic)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, dynamic_two_ints, 2)
        || !match(state, dynamic_two_doubles, 4)
        || !match(state, dynamic_two_strings, 6)
        || !match(state, dynamic_one_foo, 7)
        || !match(state, dynamic_one_bar, 8))
      break;
  }
}

BENCHMARK_REGISTER_F(Messages, MatchDynamic);

BENCHMARK_MAIN();

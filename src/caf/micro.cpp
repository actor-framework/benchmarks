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

// this file contains some micro benchmarks
// for various CAF implementation details

#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace caf;

using mscount = int_least64_t;

#define CAF_BENCH_START(bname, barg)                                           \
  auto t1 = std::chrono::high_resolution_clock::now()

#define CAF_BENCH_DONE()                                                       \
  std::chrono::duration_cast<std::chrono::milliseconds>(                       \
    std::chrono::high_resolution_clock::now() - t1).count()

namespace {

constexpr size_t num_iterations_per_bench = 1000000;
constexpr size_t num_bench_runs = 10;
size_t s_invoked = 0;

} // namespace <anonymous>

template <class F, class... Ts>
void run_bench(const char* name, const char* desc, F fun, Ts&&... args) {
  cout << name << " [" << num_iterations_per_bench << " iterations, "
       << num_bench_runs << " runs, msec] " << desc << endl;
  for (size_t i = 0; i < num_bench_runs; ++i) {
    cout << fun(std::forward<Ts>(args)...) << endl;
  }
}

// returns nr. of ms
mscount message_creation_native() {
  CAF_BENCH_START(message_creation_native, num);
  message msg = make_message(size_t{0});
  for (size_t i = 0; i < num_iterations_per_bench; ++i) {
    msg = make_message(msg.get_as<size_t>(0) + 1);
  }
  if (msg.get_as<size_t>(0) != num_iterations_per_bench) {
    std::cerr << "wrong result, found " << msg.get_as<size_t>(0)
              << ", expected " << num_iterations_per_bench << std::endl;
  }
  return CAF_BENCH_DONE();
}

mscount message_creation_dynamic() {
  CAF_BENCH_START(message_creation_dynamic, num);
  message_builder mb;
  message msg = mb.append(size_t{0}).to_message();
  for (size_t i = 0; i < num_iterations_per_bench; ++i) {
    mb.clear();
    msg = mb.append(msg.get_as<size_t>(0) + 1).to_message();
  }
  if (msg.get_as<size_t>(0) != num_iterations_per_bench) {
    std::cerr << "wrong result, found " << msg.get_as<size_t>(0)
              << ", expected " << num_iterations_per_bench << std::endl;
  }
  return CAF_BENCH_DONE();
}

mscount match_performance(behavior& bhvr, std::vector<message>& mvec) {
  CAF_BENCH_START(message_creation_native, num);
  for (size_t i = 0; i < num_iterations_per_bench; ++i) {
    for (size_t j = 0; j < mvec.size(); ++j) {
      s_invoked = 0;
      bhvr(mvec[j]);
      if (s_invoked != (j + 1) * 2) {
        cerr << "wrong handler called" << endl;
        return -1;
      }
    }
  }
  return CAF_BENCH_DONE();
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

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(foo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(bar)

void run_match_bench_with_builtin_only() {
  std::vector<message> v1{make_message(1, 2),
                          make_message(1.0, 2.0),
                          make_message("hi", "there")};
  std::vector<message> v2;
  message_builder mb;
  v2.push_back(mb.append(1).append(2).to_message());
  mb.clear();
  v2.push_back(mb.append(1.0).append(2.0).to_message());
  mb.clear();
  v2.push_back(mb.append("hi").append("there").to_message());
  mb.clear();
  std::vector<std::vector<message>> messages_vec{std::move(v1), std::move(v2)};
  behavior bhvr{
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
    }
  };
  std::vector<const char*> descs{"using make_message", "using message builder"};
  for (size_t i = 0; i < 2; ++i) {
    run_bench("match builtin types", descs[i],
              match_performance, bhvr, messages_vec[i]);
  }
}

void run_match_bench_with_userdefined_types() {
  std::vector<message> v1{make_message(foo{1, 2}),
                          make_message(bar{foo{1, 2}, "hello"})};
  std::vector<message> v2;
  message_builder mb;
  v2.push_back(mb.append(foo{1, 2}).to_message());
  mb.clear();
  v2.push_back(mb.append(bar{foo{1, 2}, "hello"}).to_message());
  mb.clear();
  std::vector<std::vector<message>> messages_vec{std::move(v1), std::move(v2)};
  behavior bhvr{
    [&](int) {
      s_invoked = 1;
    },
    [&](const foo&) {
      s_invoked = 2;
    },
    [&](double) {
      s_invoked = 3;
    },
    [&](const bar&) {
      s_invoked = 4;
    }
  };
  std::vector<const char*> descs{"using make_message", "using message builder"};
  for (size_t i = 0; i < 2; ++i) {
    run_bench("match user-defined types", descs[i],
              match_performance, bhvr, messages_vec[i]);
  }
}

int main() {
  run_bench("message creation (native)", "", message_creation_native);
  run_bench("message creation (dynamic)", "", message_creation_dynamic);
  run_match_bench_with_builtin_only();
  run_match_bench_with_userdefined_types();
}

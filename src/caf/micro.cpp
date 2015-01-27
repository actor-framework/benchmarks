/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

// returns nr. of ms
mscount message_creation(size_t num) {
  CAF_BENCH_START(message_creation, num);
  message msg = make_message(size_t{0});
  for (size_t i = 0; i < num; ++i) {
    msg = make_message(msg.get_as<size_t>(0) + 1);
  }
  if (msg.get_as<size_t>(0) != num) {
    std::cerr << "wrong result, found " << msg.get_as<size_t>(0)
              << ", expected " << num << std::endl;
  }
  return CAF_BENCH_DONE();
}

mscount match_performance_builtin_only(size_t num) {
  std::vector<message> messages{make_message(1, 2),
                                make_message(1.0, 2.0),
                                make_message("hi", "there")};
  size_t invoked = 0;
  auto bhvr = behavior{
    [&](int) {
      invoked = 1;
    },
    [&](int, int) {
      invoked = 2;
    },
    [&](double) {
      invoked = 3;
    },
    [&](double, double) {
      invoked = 4;
    },
    [&](const std::string&) {
      invoked = 5;
    },
    [&](const std::string&, const std::string&) {
      invoked = 6;
    }
  };
  CAF_BENCH_START(message_creation, num);
  for (size_t i = 0; i < num; ++i) {
    for (size_t j = 0; j < messages.size(); ++j) {
      invoked = 0;
      bhvr(messages[j]);
      if (invoked != (j + 1) * 2) {
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

mscount match_performance_with_userdefined_types(size_t num) {
  std::vector<message> messages{make_message(foo{1, 2}),
                                make_message(bar{foo{1, 2}, "hello"})};
  size_t invoked = 0;
  auto bhvr = behavior{
    [&](int) {
      invoked = 1;
    },
    [&](const foo&) {
      invoked = 2;
    },
    [&](double) {
      invoked = 3;
    },
    [&](const bar&) {
      invoked = 4;
    }
  };
  CAF_BENCH_START(message_creation, num);
  for (size_t i = 0; i < num; ++i) {
    for (size_t j = 0; j < messages.size(); ++j) {
      invoked = 0;
      bhvr(messages[j]);
      if (invoked != (j + 1) * 2) {
        cerr << "wrong handler called: expected " << ((j + 1) * 2)
             << ", found " << invoked << endl;
        return -1;
      }
    }
  }
  return CAF_BENCH_DONE();
}

int main() {
  cout << "message_creation bench(1M): 10x, ms" << endl;
  for (int i = 0; i < 10; ++i) {
    cout << message_creation(1000000) << endl;
  }
  cout << "match_performance_builtin_only bench(1M): 10x, ms" << endl;
  for (int i = 0; i < 10; ++i) {
    cout << match_performance_builtin_only(1000000) << endl;
  }
  announce<foo>("foo", &foo::a, &foo::b);
  announce<bar>("bar", &bar::a, &bar::b);
  cout << "match_performance_with_userdefined_types bench(1M): 10x, ms" << endl;
  for (int i = 0; i < 10; ++i) {
    cout << match_performance_with_userdefined_types(1000000) << endl;
  }
}

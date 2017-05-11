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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef BENCHMARK_HELPER_HPP
#define BENCHMARK_HELPER_HPP

#include <vector>
#include <limits>
#include <random>

class pseudo_random {
public:
  pseudo_random(long seed)
      : value_(seed) {
    // nop
  }

  pseudo_random() = default;

  void set_seed(long seed) {
    value_ = seed;
  }

  int next_int() {
    return next_long();
  }

  int next_int(int exclusive_max) {
    return next_long() % exclusive_max;
  }

  long long next_long() {
    value_ = ((value_ * 1309) + 13849) & 65535;
    return value_;
  }

  double next_double() {
    return 1.0 / (next_long() +1);
  }

private:
  long value_ = 74755; 
};
#endif // BENCHMARK_HELPER_HPP

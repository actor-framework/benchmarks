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

#include <list>
#include <string>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using msg1_atom = atom_constant<atom("msg1")>;
using msg2_atom = atom_constant<atom("msg2")>;
using msg3_atom = atom_constant<atom("msg3")>;
using msg4_atom = atom_constant<atom("msg4")>;
using msg5_atom = atom_constant<atom("msg5")>;
using msg6_atom = atom_constant<atom("msg6")>;

void usage() {
  cout << "usage: matching (cow_tuple|object_array) NUM_LOOPS" << endl;
}

enum impl_type { static_tuple, dynamic_tuple };

optional<impl_type> implproj(const string& str) {
  if (str == "cow_tuple")
    return static_tuple;
  if (str == "object_array")
    return dynamic_tuple;
  return none;
}

int main(int argc, char** argv) {
  int result = 1;
  auto res = message_builder{argv + 1, argv + argc}.apply({
    [&](const std::string& impl_str, const std::string& arg) {
      auto impl_opt = implproj(impl_str);
      if (!impl_opt)
        return;
      auto impl = *impl_opt;
      auto num_loops = std::stoi(arg);
      result = 0;
      message m1;
      message m2;
      message m3;
      message m4;
      message m5;
      message m6;
      if (impl == static_tuple) {
        m1 = make_message(atom("msg1"), 0);
        m2 = make_message(atom("msg2"), 0.0);
        m3 = make_message(atom("msg3"), list<int>{0});
        m4 = make_message(atom("msg4"), 0, "0");
        m5 = make_message(atom("msg5"), 0, 0, 0);
        m6 = make_message(atom("msg6"), 0, 0.0, "0");
      } else {
        message_builder mb;
        m1 = mb.append(atom("msg1")).append(0).to_message();
        mb.clear();
        m2 = mb.append(atom("msg2")).append(0.0).to_message();
        mb.clear();
        m3 = mb.append(atom("msg3")).append(list<int>{0}).to_message();
        mb.clear();
        m4 = mb.append(atom("msg4")).append(0).append("0").to_message();
        mb.clear();
        m5 = mb.append(atom("msg5")).append(0).append(0).append(0).to_message();
        mb.clear();
        m6 = mb.append(atom("msg6")).append(0).append(0.0).append("0").to_message();
      }
      int64_t m1matched = 0;
      int64_t m2matched = 0;
      int64_t m3matched = 0;
      int64_t m4matched = 0;
      int64_t m5matched = 0;
      int64_t m6matched = 0;
      message_handler part_fun{
        [&](msg1_atom, int) { ++m1matched; },
        [&](msg2_atom, double) { ++m2matched; },
        [&](msg3_atom, list<int>) { ++m3matched; },
        [&](msg4_atom, int, string) { ++m4matched; },
        [&](msg5_atom, int, int, int) { ++m5matched; },
        [&](msg6_atom, int, double, string) { ++m6matched; }
      };
      for (int64_t i = 0; i < num_loops; ++i) {
          part_fun(m1);
          part_fun(m2);
          part_fun(m3);
          part_fun(m4);
          part_fun(m5);
          part_fun(m6);
      }
      assert(m1matched == num_loops);
      assert(m2matched == num_loops);
      assert(m3matched == num_loops);
      assert(m4matched == num_loops);
      assert(m5matched == num_loops);
      assert(m6matched == num_loops);
      result = 0;
    }
  });
  if (!res)
    usage();
  return result;
}

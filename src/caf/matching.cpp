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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <string>

#include "caf/all.hpp"

#if CAF_VERSION < 1800

using msg1_atom = caf::atom_constant<caf::atom("msg1")>;
using msg2_atom = caf::atom_constant<caf::atom("msg2")>;
using msg3_atom = caf::atom_constant<caf::atom("msg3")>;
using msg4_atom = caf::atom_constant<caf::atom("msg4")>;
using msg5_atom = caf::atom_constant<caf::atom("msg5")>;
using msg6_atom = caf::atom_constant<caf::atom("msg6")>;
static constexpr msg1_atom msg1_atom_v = msg1_atom::value;
static constexpr msg2_atom msg2_atom_v = msg2_atom::value;
static constexpr msg3_atom msg3_atom_v = msg3_atom::value;
static constexpr msg4_atom msg4_atom_v = msg4_atom::value;
static constexpr msg5_atom msg5_atom_v = msg5_atom::value;
static constexpr msg6_atom msg6_atom_v = msg6_atom::value;

#else

CAF_BEGIN_TYPE_ID_BLOCK(matching, first_custom_type_id)

  CAF_ADD_TYPE_ID(matching, (std::list<int32_t>) );

  CAF_ADD_ATOM(matching, msg1_atom);
  CAF_ADD_ATOM(matching, msg2_atom);
  CAF_ADD_ATOM(matching, msg3_atom);
  CAF_ADD_ATOM(matching, msg4_atom);
  CAF_ADD_ATOM(matching, msg5_atom);
  CAF_ADD_ATOM(matching, msg6_atom);

CAF_END_TYPE_ID_BLOCK(matching)

#endif

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::string;
using std::vector;

using namespace caf;

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
  if (argc != 3)
    return EXIT_FAILURE;
#if CAF_VERSION >= 1800
  init_global_meta_objects<caf::id_block::matching>();
#endif
  impl_type impl;
  if (auto impl_opt = implproj(argv[1])) {
    impl = *impl_opt;
  } else {
    usage();
    return EXIT_FAILURE;
  }
  int64_t num_loops = 0;
  try {
    num_loops = std::stoi(argv[2]);
  } catch (...) {
    usage();
    return EXIT_FAILURE;
  }
  message m1;
  message m2;
  message m3;
  message m4;
  message m5;
  message m6;
  if (impl == static_tuple) {
    m1 = make_message(msg1_atom_v, 0);
    m2 = make_message(msg2_atom_v, 0.0);
    m3 = make_message(msg3_atom_v, list<int32_t>{0});
    m4 = make_message(msg4_atom_v, 0, "0");
    m5 = make_message(msg5_atom_v, 0, 0, 0);
    m6 = make_message(msg6_atom_v, 0, 0.0, "0");
  } else {
    message_builder mb;
    m1 = mb.append(msg1_atom_v).append(0).to_message();
    mb.clear();
    m2 = mb.append(msg2_atom_v).append(0.0).to_message();
    mb.clear();
    m3 = mb.append(msg3_atom_v).append(list<int32_t>{0}).to_message();
    mb.clear();
    m4 = mb.append(msg4_atom_v).append(0).append("0").to_message();
    mb.clear();
    m5 = mb.append(msg5_atom_v).append(0).append(0).append(0).to_message();
    mb.clear();
    m6 = mb.append(msg6_atom_v).append(0).append(0.0).append("0").to_message();
  }
  int64_t m1matched = 0;
  int64_t m2matched = 0;
  int64_t m3matched = 0;
  int64_t m4matched = 0;
  int64_t m5matched = 0;
  int64_t m6matched = 0;
  message_handler part_fun{
    [&](msg1_atom, int32_t) { ++m1matched; },
    [&](msg2_atom, double) { ++m2matched; },
    [&](msg3_atom, list<int32_t>) { ++m3matched; },
    [&](msg4_atom, int32_t, string) { ++m4matched; },
    [&](msg5_atom, int32_t, int32_t, int32_t) { ++m5matched; },
    [&](msg6_atom, int32_t, double, string) { ++m6matched; }};
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
  return EXIT_SUCCESS;
}

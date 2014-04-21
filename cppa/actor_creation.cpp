/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utility.hpp"
#include "backward_compatibility.hpp"

using namespace std;
using namespace cppa;

namespace { uint32_t s_num; }

struct testee : sb_actor<testee> {
    behavior init_state;
    testee(const actor_hdl& parent) {
        init_state = (
            on(atom("spread"), (uint32_t) 1) >> [=] {
                send(parent, atom("result"), (uint32_t) 1);
                quit();
            },
            on(atom("spread"), arg_match) >> [=](uint32_t x) {
                any_tuple msg = make_cow_tuple(atom("spread"), x - 1);
                send_tuple(spawn<testee>(this), msg);
                send_tuple(spawn<testee>(this), msg);
                become (
                    on(atom("result"), arg_match) >> [=](uint32_t r1) {
                        become (
                            on(atom("result"), arg_match) >> [=](uint32_t r2) {
                                if (parent == invalid_actor) {
                                    uint32_t res = 2 + r1 + r2;
                                    uint32_t expected = (1 << s_num);
                                    if (res != expected) {
                                        cerr << "expected: " << expected
                                             << ", found: " << res
                                             << endl;
                                        exit(42);
                                    }
                                }
                                else send(parent, atom("result"), 1 + r1 + r2);
                                quit();
                            }
                        );
                    }
                );
            }
        );
    }
};

void stacked_testee(BLOCKING_SELF_ARG actor_hdl parent) {
    SELF_PREFIX receive (
        on(atom("spread"), (uint32_t) 1) >> [&]() {
            SELF_PREFIX send(parent, atom("result"), (uint32_t) 1);
        },
        on(atom("spread"), arg_match) >> [&](uint32_t x) {
            auto child1 = SELF_PREFIX spawn<blocking_api>(stacked_testee, self);
            auto child2 = SELF_PREFIX spawn<blocking_api>(stacked_testee, self);
            any_tuple msg = make_cow_tuple(atom("spread"), x - 1);
            SELF_PREFIX send_tuple(child1, msg);
            SELF_PREFIX send_tuple(child2, msg);
            SELF_PREFIX receive (
                on(atom("result"), arg_match) >> [&](uint32_t v1) {
                    SELF_PREFIX receive (
                        on(atom("result"), arg_match) >> [&](uint32_t v2) {
                            SELF_PREFIX send(parent, atom("result"), v1 + v2);
                        }
                    );
                }
            );
        }
    );
}

void usage() {
    cout << "usage: actor_creation [--stacked] POW" << endl
         << "       creates 2^POW actors" << endl
         << endl;
}

int main(int argc, char** argv) {
    vector<string> args(argv + 1, argv + argc);
    match (args) (
        on("--stacked", spro<uint32_t>) >> [](uint32_t num) {
            s_num = num;
            anon_send(spawn<blocking_api>(stacked_testee, invalid_actor), atom("spread"), num);
        },
        on(spro<uint32_t>) >> [](uint32_t num) {
            s_num = num;
            anon_send(spawn<testee>(invalid_actor), atom("spread"), num);
        },
        others() >> usage
    );
    await_all_actors_done();
    shutdown();
    return 0;
}

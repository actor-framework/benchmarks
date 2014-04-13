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

struct fsm_receiver : sb_actor<fsm_receiver> {
    uint64_t m_value;
    behavior init_state;
    fsm_receiver(uint64_t max) : m_value(0) {
        init_state = (
            on(atom("msg")) >> [=] {
                if (++m_value == max) {
                    quit();
                }
            }
        );
    }
};

void receiver(BLOCKING_SELF_ARG uint64_t max) {
    uint64_t value;
    SELF_PREFIX receive_while (gref(value) < max) (
        on(atom("msg")) >> [&] {
            ++value;
        }
    );
}

void sender(actor_hdl whom, uint64_t count) {
    if (!whom) return;
    any_tuple msg = make_cow_tuple(atom("msg"));
    for (uint64_t i = 0; i < count; ++i) {
        anon_send_tuple(whom, msg);
    }
}

void usage() {
    cout << "usage: mailbox_performance "
            "[--stacked] NUM_THREADS MSGS_PER_THREAD" << endl
         << endl;
    exit(1);
}

enum impl_type { stacked, event_based };

void run(impl_type impl, uint64_t num_sender, uint64_t num_msgs) {
    auto total = num_sender * num_msgs;
    auto testee = (impl == stacked) ? spawn<blocking_api>(receiver, total)
                                    : spawn<fsm_receiver>(total);
    for (uint64_t i = 0; i < num_sender; ++i) {
        spawn(sender, testee, num_msgs);
    }
    /*
    vector<thread> senders;
    for (uint64_t i = 0; i < num_sender; ++i) {
        senders.emplace_back(sender, testee, num_msgs);
    }
    for (auto& s : senders) {
        s.join();
    }
    */
}

int main(int argc, char** argv) {
    using namespace std::placeholders;
    vector<string> args(argv + 1, argv + argc);
    std::function<void (uint64_t,uint64_t)> f;
    if (!args.empty() && args.front() == "--stacked") {
        f = std::bind(run, stacked, _1, _2);
        args.erase(args.begin());
    }
    else f = std::bind(run, event_based, _1, _2);
    match (args) (
        on(spro<uint64_t>, spro<uint64_t>) >> f,
        others() >> usage
    );
    await_all_actors_done();
    shutdown();
    return 0;
}

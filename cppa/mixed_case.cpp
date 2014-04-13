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
#include <functional>

#include "utility.hpp"
#include "backward_compatibility.hpp"

using namespace std;
using namespace cppa;

typedef vector<uint64_t> factors;

constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;
constexpr uint64_t s_task_n = s_factor1 * s_factor2;

void check_factors(const factors& vec) {
    assert(vec.size() == 2);
    assert(vec[0] == s_factor1);
    assert(vec[1] == s_factor2);
#   ifdef NDEBUG
    static_cast<void>(vec);
#   endif
}

struct fsm_worker : sb_actor<fsm_worker> {
    actor_hdl mc;
    behavior init_state;
    fsm_worker(const actor_hdl& msgcollector) : mc(msgcollector) {
        init_state = (
            on<atom("calc"), uint64_t>() >> [=](uint64_t what) {
                send(mc, atom("result"), factorize(what));
            },
            on(atom("done")) >> [=]() {
                quit();
            }
        );
    }
};

struct fsm_chain_link : sb_actor<fsm_chain_link> {
    actor_hdl next;
    behavior init_state;
    fsm_chain_link(const actor_hdl& n) : next(n) {
        init_state = (
            on<atom("token"), int>() >> [=](int v) {
                send_tuple(next, std::move(last_dequeued()));
                if (v == 0) quit();
            }
        );
    }
};

struct fsm_chain_master : sb_actor<fsm_chain_master> {
    int iteration;
    actor_hdl mc;
    actor_hdl next;
    actor_hdl worker;
    behavior init_state;
    void new_ring(int ring_size, int initial_token_value) {
        send(worker, atom("calc"), s_task_n);
        next = this;
        for (int i = 1; i < ring_size; ++i) {
            next = spawn<fsm_chain_link>(next);
        }
        send(next, atom("token"), initial_token_value);
    }
    fsm_chain_master(actor_hdl msgcollector) : iteration(0), mc(msgcollector) {
        init_state = (
            on(atom("init"), arg_match) >> [=](int rs, int itv, int n) {
                worker = spawn<fsm_worker>(msgcollector);
                iteration = 0;
                new_ring(rs, itv);
                become (
                    on(atom("token"), 0) >> [=]() {
                        if (++iteration < n) {
                            new_ring(rs, itv);
                        }
                        else {
                            send(worker, atom("done"));
                            send(mc, atom("masterdone"));
                            quit();
                        }
                    },
                    on<atom("token"), int>() >> [=](int v) {
                        send(next, atom("token"), v - 1);
                    }
                );
            }
        );
    }
};

struct fsm_supervisor : sb_actor<fsm_supervisor> {
    int left;
    behavior init_state;
    fsm_supervisor(int num_msgs) : left(num_msgs) {
        init_state = (
            on(atom("masterdone")) >> [=]() {
                if (--left == 0) quit();
            },
            on<atom("result"), factors>() >> [=](const factors& vec) {
                check_factors(vec);
                if (--left == 0) quit();
            }
        );
    }
};

void chain_link(BLOCKING_SELF_ARG actor_hdl next) {
    bool done = false;
    SELF_PREFIX do_receive (
        on<atom("token"), int>() >> [&](int v) {
            if (v == 0) {
                done = true;
            }
            SELF_PREFIX send_tuple(next, std::move(self->last_dequeued()));
        }
    )
    .until([&]() { return done == true; });
}

void worker_fun(BLOCKING_SELF_ARG actor_hdl msgcollector) {
    bool done = false;
    SELF_PREFIX do_receive (
        on<atom("calc"), uint64_t>() >> [&](uint64_t what) {
            SELF_PREFIX send(msgcollector, atom("result"), factorize(what));
        },
        on(atom("done")) >> [&]() {
            done = true;
        }
    )
    .until([&]() { return done == true; });
}

actor_hdl new_ring(actor_hdl next, int ring_size) {
    for (int i = 1; i < ring_size; ++i) next = spawn<blocking_api>(chain_link, next);
    return next;
}

void chain_master(BLOCKING_SELF_ARG actor_hdl msgcollector) {
    auto worker = SELF_PREFIX spawn<blocking_api>(worker_fun, msgcollector);
    SELF_PREFIX receive (
        on(atom("init"), arg_match) >> [&](int rs, int itv, int n) {
            int iteration = 0;
            auto next = new_ring(self, rs);
            SELF_PREFIX send(next, atom("token"), itv);
            SELF_PREFIX send(worker, atom("calc"), s_task_n);
            SELF_PREFIX do_receive (
                on<atom("token"), int>() >> [&](int v) {
                    if (v == 0) {
                        if (++iteration < n) {
                            next = new_ring(self, rs);
                            SELF_PREFIX send(next, atom("token"), itv);
                            SELF_PREFIX send(worker, atom("calc"), s_task_n);
                        }
                    }
                    else {
                        SELF_PREFIX send(next, atom("token"), v - 1);
                    }
                }
            )
            .until([&]() { return iteration == n; });
        }
    );
    SELF_PREFIX send(msgcollector, atom("masterdone"));
    SELF_PREFIX send(worker, atom("done"));
}

void supervisor(BLOCKING_SELF_ARG int num_msgs) {
    SELF_PREFIX do_receive (
        on(atom("masterdone")) >> [&]() {
            --num_msgs;
        },
        on<atom("result"), factors>() >> [&](const factors& vec) {
            --num_msgs;
            check_factors(vec);
        }
    )
    .until([&]() { return num_msgs == 0; });
}

template<typename F>
void run_test(F spawn_impl,
              int num_rings, int ring_size,
              int initial_token_value, int repetitions) {
    std::vector<actor_hdl> masters; // of the universe
    // each master sends one masterdone message and one
    // factorization is calculated per repetition
    //auto supermaster = spawn(supervisor, num_rings+repetitions);
    for (int i = 0; i < num_rings; ++i) {
        masters.push_back(spawn_impl());
        anon_send(masters.back(),
                  atom("init"),
                  ring_size,
                  initial_token_value,
                  repetitions);
    }
    await_all_actors_done();
}

void usage() {
    cout << "usage: mailbox_performance "
            "[--stacked] (num rings) (ring size) "
            "(initial token value) (repetitions)"
         << endl
         << endl;
    exit(1);
}

enum impl_type { stacked, event_based };

void run(impl_type impl, int num_rings, int ring_size, int initial_token_value, int repetitions) {
    int num_msgs = num_rings + (num_rings * repetitions);
    auto sv = (impl == event_based) ? spawn<fsm_supervisor>(num_msgs)
                                    : spawn<blocking_api>(supervisor, num_msgs);
    if (impl == event_based) {
        run_test([sv] { return spawn<fsm_chain_master>(sv); },
                 num_rings, ring_size, initial_token_value, repetitions);
    }
    else {
        run_test([sv] { return spawn<blocking_api>(chain_master, sv); },
                 num_rings, ring_size, initial_token_value, repetitions);
    }
}

int main(int argc, char** argv) {
    using namespace std::placeholders;
    announce<factors>();
    std::vector<std::string> args{argv + 1, argv + argc};
    std::function<void (int,int,int,int)> f;
    if (!args.empty() && args.front() == "--stacked") {
        f = std::bind(run, stacked, _1, _2, _3, _4);
        args.erase(args.begin());
    }
    else f = std::bind(run, event_based, _1, _2, _3, _4);
    match(args) (
        on(spro<int>, spro<int>, spro<int>, spro<int>) >> f,
        others() >> usage
    );
    return 0;
}

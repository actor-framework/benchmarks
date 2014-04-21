#include <Theron/Theron.h> 

#include <thread>
#include <cstdint>
#include <iostream>
#include <functional>

#define THERON_BENCHMARK
#include "utility.hpp"

using namespace std;
using namespace Theron;

typedef unique_ptr<Actor> actor_ptr;

constexpr uint64_t s_task_n = uint64_t(86028157)*329545133;
constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;

typedef vector<uint64_t> factors;

struct calc_msg { uint64_t value; };
struct result_msg { factors result; };
struct token_msg { int value; };
struct init_msg { int ring_size; int token_value; int iterations; };
struct master_done { };
struct worker_done { };


struct worker : Actor {
    void handle_calc(const calc_msg& msg, Address from) {
        factorize(msg.value);
    }
    void handle_master_done(const master_done&, Address from) {
        Send(worker_done(), from);
    }
    worker(Framework& f) : Actor(f) {
        RegisterHandler(this, &worker::handle_calc);
        RegisterHandler(this, &worker::handle_master_done);
    }
};

struct chain_link : Actor {
    Address next;
    void handle_token(const token_msg& msg, Address) {
        Send(msg, next);
    }
    chain_link(Framework& f, Address addr) : Actor(f), next(addr) {
        RegisterHandler(this, &chain_link::handle_token);
    }
};

struct master : Actor {
    Address mc;
    int iteration;
    int max_iterations;
    Address next;
    actor_ptr w;
    int ring_size;
    int initial_token_value;
    vector<actor_ptr> m_children;
    void new_ring() {
        m_children.clear();
        Send(calc_msg{s_task_n}, w->GetAddress());
        next = GetAddress();
        for (int i = 1; i < ring_size; ++i) {
            m_children.emplace_back(new chain_link(GetFramework(), next));
            next = m_children.back()->GetAddress();
        }
        Send(token_msg{initial_token_value}, next);
    }
    void handle_init(const init_msg& msg, Address) {
        w.reset(new worker(GetFramework()));
        iteration = 0;
        ring_size = msg.ring_size;
        initial_token_value = msg.token_value;
        max_iterations = msg.iterations;
        new_ring();
    }
    void handle_token(const token_msg& msg, Address) {
        if (msg.value == 0) {
            if (++iteration < max_iterations) {
                new_ring();
            }
            else {
                Send(master_done(), w->GetAddress());
            }
        }
        else {
            Send(token_msg{msg.value - 1}, next);
        }
    }
    void handle_worker_done(const worker_done&, Address) {
        Send(master_done(), mc);
        w.reset();
        m_children.clear();
    }
    master(Framework& f, Address mcaddr) : Actor(f), mc(mcaddr), iteration(0) {
        RegisterHandler(this, &master::handle_init);
        RegisterHandler(this, &master::handle_token);
        RegisterHandler(this, &master::handle_worker_done);
    }
};

void usage() {
    cout << "usage: mailbox_performance "
            "(num rings) (ring size) "
            "(initial token value) (repetitions)"
         << endl
         << endl;
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 5) usage();
    int num_rings = rd<int>(argv[1]);
    int ring_size = rd<int>(argv[2]);
    int inital_token_value = rd<int>(argv[3]);
    int repetitions = rd<int>(argv[4]);
    Receiver r;
    Framework framework(num_cores());
    vector<actor_ptr> masters;
    // spawn masters
    for (int i = 0; i < num_rings; ++i) {
        masters.emplace_back(new master(framework, r.GetAddress()));
    }
    // kickoff
    init_msg kickoff_msg{ring_size, inital_token_value, repetitions};
    for (auto& m : masters) {
        framework.Send(kickoff_msg, r.GetAddress(), m->GetAddress());
    }
    r.Wait(num_rings);
    masters.clear();
    return 0;
}

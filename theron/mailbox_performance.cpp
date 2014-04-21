#include <Theron/Theron.h> 

#include <thread>
#include <cstdint>
#include <iostream>
#include <functional>

#define THERON_BENCHMARK
#include "utility.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::int64_t;

using namespace Theron;

int64_t t_max = 0;

struct receiver : Actor {

    int64_t m_num;

    void handler(const int64_t&, const Address from) {
        if (++m_num == t_max)
            Send(t_max, from);
    }

    receiver(Framework& f) : Actor(f), m_num(0) {
        RegisterHandler(this, &receiver::handler);
    }

};

void sender(Framework& f, Address dest, Address waiter, int64_t num) {
    int64_t msg = 0;
    for (int64_t i = 0; i < num; ++i) f.Send(msg, waiter, dest);
}

void usage() {
    cout << "usage (num_threads) (num_messages)" << endl;
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        usage();
    }
    auto num_sender = rd<int64_t>(argv[1]);
    auto num_msgs = rd<int64_t>(argv[2]);
    Receiver r;
    t_max = num_sender * num_msgs;
    auto receiverAddr = r.GetAddress();
    Framework framework(num_cores());
    std::vector<std::thread> threads;
    { // lifetime scope of our receiving actor
        receiver testee{framework};
        for (int64_t i = 0; i < num_sender; ++i) {
            threads.push_back(std::thread(sender, std::ref(framework), testee.GetAddress(), receiverAddr, num_msgs));
        }
        r.Wait();
    }
    for (auto& t : threads) t.join();
    return 0;
}


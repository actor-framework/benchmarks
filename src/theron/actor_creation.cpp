#include <Theron/Theron.h> 

#include <thread>
#include <cstdint>
#include <iostream>
#include <functional>

#define THERON_BENCHMARK
#include "utility.hpp"

struct spread { int value; };
struct result { uint32_t value; };

using namespace std;
using namespace Theron;

typedef shared_ptr<Actor> actor_ptr;

struct testee : Actor {

    Address m_parent;
    bool m_first_result_received;
    uint32_t m_first_result;
    vector<actor_ptr> m_children;

    void spread_handler(const spread& arg, const Address parent) {
        m_parent = parent;
        if (arg.value == 1) {
            Send(result{1}, m_parent);
        }
        else {
            spread msg{arg.value-1};
            for (int i = 0; i < 2; ++i) {
                m_children.emplace_back(new testee(GetFramework()));
                Send(msg, m_children.back()->GetAddress());
            }
        }
    }

    void result_handler(const result& arg, const Address) {
        if (!m_first_result_received) {
            m_first_result_received = true;
            m_first_result = arg.value;
        }
        else {
            m_children.clear();
            Send(result{1 + m_first_result + arg.value}, m_parent);
        }
    }

    testee(Framework& f) : Actor(f), m_first_result_received(false) {
        RegisterHandler(this, &testee::spread_handler);
        RegisterHandler(this, &testee::result_handler);
    }

};

void usage() {
    cout << "usage: theron_actor_creation POW" << endl
         << "       creates 2^POW actors" << endl
         << endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        usage();
        return 1;
    }
    int num = rd<int>(argv[1]);
    Framework framework(num_cores());
    Receiver r;
    { // lifetime scope of root actor
        testee root{framework};
        framework.Send(spread{num}, r.GetAddress(), root.GetAddress());
        struct checker {
            int m_num;
            checker(int num_arg) : m_num(num_arg) { }
            void check_res(const result& arg, const Address) {
                auto expected = uint32_t{1} << m_num;
                auto res = arg.value + 1;
                if (expected != res) {
                    cout << "expected: " << expected  << ", "
                         << "received: " << res << endl;
                    exit(42);
                }
            }
        } foo{num};
        r.RegisterHandler(&foo, &checker::check_res);
        r.Wait();
    }
}

